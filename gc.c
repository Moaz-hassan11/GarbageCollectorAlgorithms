#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_REFS 10
#define MAX_OBJECTS 20

typedef struct {
    char id[10];
    int address;

    char references[MAX_REFS][10];
    int num_refs;

    int ref_count;
    bool marked;

} Object;

typedef struct {
    Object objects[MAX_OBJECTS];
    int num_objects;
} Heap;


int find_object_index(Heap* heap, const char* id) {
    for (int i = 0; i < heap->num_objects; i++) {
        if (strcmp(heap->objects[i].id, id) == 0) {
            return i;
        }
    }
    return -1;
}


void mark_objects(Heap* heap, const char* current_id) {
    int idx = find_object_index(heap, current_id);
    if (idx == -1 || heap->objects[idx].marked) {
        return;
    }
    heap->objects[idx].marked = true;
    for (int i = 0; i < heap->objects[idx].num_refs; i++) {
        mark_objects(heap, heap->objects[idx].references[i]);
    }
}

void reference_counting_gc(Heap* heap, char roots[][10], int num_roots) {
    for (int i = 0; i < heap->num_objects; i++) {
        heap->objects[i].ref_count = 0;
    }
    for (int i = 0; i < num_roots; i++) {
        int idx = find_object_index(heap, roots[i]);
        if (idx != -1) {
            heap->objects[idx].ref_count++;
        }
    }

    for (int i = 0; i < heap->num_objects; i++) {
        for (int j = 0; j < heap->objects[i].num_refs; j++) {
            int target_idx = find_object_index(heap,heap->objects[i].references[j]);
            if (target_idx != -1) {
                heap->objects[target_idx].ref_count++;
            }
        }
    }
    printf("{\"remaining_objects\":[");

    bool first = true;
    for (int i = 0; i < heap->num_objects; i++) {
        if (heap->objects[i].ref_count > 0) {
            if (!first) {
                printf(",");
            }
            printf("\"%s\"", heap->objects[i].id);
            first = false;
        }
    }
    printf("]}");
}

void mark_and_sweep_gc(Heap* heap,char roots[][10],int num_roots) {
    for (int i = 0; i < heap->num_objects; i++) {
        heap->objects[i].marked = false;
    }
    for (int i = 0; i < num_roots; i++) {
        mark_objects(heap, roots[i]);
    }
    printf("{\"remaining_objects\":[");
    bool first = true;
    for (int i = 0; i < heap->num_objects; i++) {
        if (heap->objects[i].marked) {
            if (!first) {
                printf(",");
            }
            printf("\"%s\"", heap->objects[i].id);
            first = false;
        }
    }
    printf("]}");
}

void mark_and_compact_gc(Heap* heap,char roots[][10],int num_roots) {
    for (int i = 0; i < heap->num_objects; i++) {
        heap->objects[i].marked = false;
    }
    for (int i = 0; i < num_roots; i++) {
        mark_objects(heap, roots[i]);
    }
    int next_available_address = 0;
    printf("{\"remaining_objects\":{");
    bool first = true;
    for (int i = 0; i < heap->num_objects; i++) {
        if (heap->objects[i].marked) {
            if (!first) {
                printf(",");
            }
            printf(
                "\"%s\":%d",
                heap->objects[i].id,
                next_available_address++
            );
            first = false;
        }
    }
    printf("}}");
}

void parse_combined_json(char* json_str,char roots[MAX_REFS][10],int* num_roots,Heap* heap) {
    *num_roots = 0;
    heap->num_objects = 0;
    char* roots_start = strstr(json_str, "roots");
    if (roots_start) {
        char* open_bracket = strchr(roots_start, '[');
        char* close_bracket = strchr(roots_start, ']');
        if (open_bracket && close_bracket) {
            char* p = open_bracket;
            while (p < close_bracket) {
                if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')) {
                    char temp[10] = {0};
                    int idx = 0;
                    while ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9')) {
                        if (idx < 9) {
                            temp[idx++] = *p;
                        }
                        p++;
                    }
                    temp[idx] = '\0';
                    strncpy(roots[*num_roots],temp,9);
                    roots[*num_roots][9] = '\0';
                    (*num_roots)++;
                }
                p++;
            }
        }
    }
    char* heap_start = strstr(json_str, "heap");
    if (!heap_start) {
        return;
    }
    char* search_ptr = heap_start;
    while ((search_ptr = strchr(search_ptr, '"')) != NULL) {
        search_ptr++;
        char object_id[10] = {0};
        int idx = 0;
        while ((*search_ptr >= 'A' && *search_ptr <= 'Z') || (*search_ptr >= 'a' && *search_ptr <= 'z') || (*search_ptr >= '0' && *search_ptr <= '9')) {
            if (idx < 9) {
                object_id[idx++] = *search_ptr;
            }
            search_ptr++;
        }
        object_id[idx] = '\0';
        if (strcmp(object_id, "address") == 0 ||strcmp(object_id, "references") == 0 || strlen(object_id) == 0) {
            continue;
        }
        while (*search_ptr && *search_ptr != '{') {
            search_ptr++;
        }
        if (*search_ptr != '{') {
            continue;
        }
        Object* obj = &heap->objects[heap->num_objects];
        strncpy(obj->id, object_id, 9);
        obj->id[9] = '\0';
        obj->num_refs = 0;
        obj->ref_count = 0;
        obj->marked = false;
        obj->address = 0;

        char* object_end = strchr(search_ptr, '}');
        if (!object_end) {
            continue;
        }
        char* addr_ptr = strstr(search_ptr, "address");
        if (addr_ptr && addr_ptr < object_end) {
            addr_ptr = strchr(addr_ptr, ':');

            if (addr_ptr) {
                sscanf(addr_ptr + 1, "%d", &obj->address);
            }
        }
        char* refs_ptr = strstr(search_ptr, "references");
        if (refs_ptr && refs_ptr < object_end) {
            refs_ptr = strchr(refs_ptr, '[');
            if (refs_ptr) {
                refs_ptr++;
                while (*refs_ptr && *refs_ptr != ']') {
                    if ((*refs_ptr >= 'A' && *refs_ptr <= 'Z') || (*refs_ptr >= 'a' && *refs_ptr <= 'z')) {
                        char ref_id[10] = {0};
                        int r = 0;
                        while ((*refs_ptr >= 'A' && *refs_ptr <= 'Z') ||(*refs_ptr >= 'a' && *refs_ptr <= 'z') ||(*refs_ptr >= '0' && *refs_ptr <= '9')
                        ) {
                            if (r < 9) {
                                ref_id[r++] = *refs_ptr;
                            }
                            refs_ptr++;
                        }
                        ref_id[r] = '\0';
                        strncpy(obj->references[obj->num_refs],ref_id,9);
                        obj->references[obj->num_refs][9] = '\0';
                        obj->num_refs++;
                    }
                    refs_ptr++;
                }
            }
        }
        heap->num_objects++;
        search_ptr = object_end + 1;
    }
}

int main() {
    FILE* file = fopen("gc_testcases.csv", "r");
    if (!file) {
        printf("Error: Could not open gc_testcases.csv\n");
        return 1;
    }
    char line[4096];
    fgets(line, sizeof(line), file);
    printf("testcase_id,reference_counting-json,mark_sweep_json,mark_compact.json\n");
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = '\0';
        char testcase_id[20] = {0};
        char input_json[4000] = {0};
        char* first_comma = strchr(line, ',');

        if (!first_comma) {
            continue;
        }

        int id_len = first_comma - line;
        strncpy(testcase_id, line, id_len);
        testcase_id[id_len] = '\0';
        char* first_quote = strchr(first_comma, '"');
        char* last_quote = strrchr(line, '"');

        if (!first_quote ||!last_quote ||last_quote <= first_quote) {
            continue;
        }
        int json_len = last_quote - first_quote - 1;
        strncpy(input_json,first_quote + 1,json_len);
        input_json[json_len] = '\0';
        char roots[MAX_REFS][10];
        int num_roots;

        Heap heap_rc;
        Heap heap_ms;
        Heap heap_mc;

        parse_combined_json(input_json,roots,&num_roots,&heap_rc);
        memcpy(&heap_ms, &heap_rc, sizeof(Heap));
        memcpy(&heap_mc, &heap_rc, sizeof(Heap));
        printf("%s,", testcase_id);
        reference_counting_gc(&heap_rc,roots,num_roots);

        printf(",");
        mark_and_sweep_gc(&heap_ms,roots,num_roots);
        printf(",");

        mark_and_compact_gc(&heap_mc,roots,num_roots);

        printf("\n");
    }
    fclose(file);
    return 0;
}