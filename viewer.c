#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include "cJSON.h" // Nicht vergessen einzubinden!

typedef struct {
    int id;
    char date[11];
    char deadline[11];
    char subject[32];
    char content[256];
    int done;
} Task;

void init_env(struct termios *orig);
void deactivate_env(struct termios *orig);
void print_file(const char *filepath);

Task* get_tasks_for_subject(char subject, int *count);
Task* get_all_tasks(int *total_count);
void append_task(Task **all_tasks, int *total);
void delete_task(Task *all_tasks, int *total, int del);
void edit_task(Task **all_tasks, int *total, int edit_file);
void mark_as_x(Task *all_tasks, int *total, int edit_file);

int cmp_subject(const void *a, const void *b);
int cmp_date(const void *a, const void *b);
int cmp_deadline(const void *a, const void *b);

int subject_matches_code(const char *subject, char code);
void save_all_tasks(Task *all_tasks, int total);

void init_desktop();

void enable_input_echo(void);
void disable_input_echo(void);

int main() {
    struct termios orig;
    init_env(&orig);
    init_desktop();
    deactivate_env(&orig);
    return 0;
}


void init_desktop() {
    int total = 0;
    Task *all_tasks = get_all_tasks(&total);    // enthält troz free speicher in get_all_tasks eine cpoie von allem also alle tasks

    int sorted_selected = 1;
    int sort_all = 3;
    char *sorted[] = {"Subjsect", "Date", "Deadline"};

    int help = 0;

    int selected_folder = 0;

    while (1) {
        fprintf(stderr,"\033[H\033[J");
        fprintf(stderr,"Gefundene Aufgaben insgesamt: %d\n", total);
        fprintf(stderr,"-----------------------------------\n");
        fprintf(stderr,"Sorted by: %s\n", sorted[sorted_selected]);
        fprintf(stderr,"-----------------------------------\n");
        for (int i = 0; i < total; i++) {
            if (i == selected_folder) {         // "\033[7m %s \033[0m\n
                fprintf(stderr," -> \033[7m[%s] %s (Bis: %s) - %s\033[0m\n",
                    all_tasks[i].done ? "X" : " ",
                    all_tasks[i].subject,
                    all_tasks[i].deadline,
                    all_tasks[i].content);
            }else{
                fprintf(stderr,"[%s] %s (Bis: %s) - %s\n",
                    all_tasks[i].done ? "X" : " ",
                    all_tasks[i].subject,
                    all_tasks[i].deadline,
                    all_tasks[i].content);
            }
        }

        char c;
        if (read(STDIN_FILENO, &c, 1) <= 0) break;
        //quitting
        if (c == 'q' || c == 'Q') {
            selected_folder = -1;
            break;
        }
        // HELP
        if (c == '1') {
            help = 1;
        }
        while (help == 1) {
            fprintf(stderr,"\033[H\033[J");
            print_file ("assets/help.txt");
            char c;
            if (read(STDIN_FILENO, &c, 1) <= 0) break;
            if (c == '1') {
                help = 0;
            }
        }
        //basic navigation
        if (c == '\033') {
            char key_queue[2];
            if (read(STDIN_FILENO, &key_queue[0], 1) > 0 && read(STDIN_FILENO, &key_queue[1], 1) > 0) {
                if (key_queue[0] == '[') {
                    if (key_queue[1] == 'A') {
                        if (total > 0) selected_folder = (selected_folder - 1 + total) % total;
                    } else if (key_queue[1] == 'B') {
                        if (total > 0) selected_folder = (selected_folder + 1) % total;
                    }
                }
            }
        }
        // change sorting
        if (c == '2') {
            sorted_selected = (sorted_selected + 1) % sort_all;
            if (sorted_selected == 0) qsort(all_tasks, total, sizeof(Task), cmp_subject);
            else if (sorted_selected == 1) qsort(all_tasks, total, sizeof(Task), cmp_date);
            else qsort(all_tasks, total, sizeof(Task), cmp_deadline);
        }
        // add
        if (c == 'a' || c == 'A') {
            append_task(&all_tasks, &total);
            save_all_tasks(all_tasks, total);
        }
        // delete
        if (c == 'x' || c == 'X') {
            if (total > 0) {
                delete_task(all_tasks, &total, selected_folder);
                if (selected_folder >= total) selected_folder = total - 1;
                save_all_tasks(all_tasks, total);
            }
        }
        // edit mark as x
        if (c == 'e' || c == 'E') {
            if (total > 0) {
                mark_as_x(all_tasks, &total, selected_folder);
                save_all_tasks(all_tasks, total);
            }
        }
        // edit given info
        if (c == 'i' || c == 'I') {
            if (total > 0) {
                edit_task(&all_tasks, &total, selected_folder);
                save_all_tasks(all_tasks, total);
            }
        }
    }

    free(all_tasks);
}

// UI Terminal-Modus
void init_env(struct termios *orig) {
    struct termios raw;
    tcgetattr(STDIN_FILENO, orig);
    raw = *orig;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void deactivate_env(struct termios *orig) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, orig);
}

void print_file(const char *filepath) {
    FILE *file = fopen(filepath, "r");
    if (!file) return;
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), file)) {
        fputs(buffer, stderr);
    }
    fclose(file);
}

// Einzelnes Fach einlesen
Task* get_tasks_for_subject(char subject, int *count) {
    char filepath[64];

    switch (subject) {
        case 'm':
            snprintf(filepath, sizeof(filepath), "subjects_json/math.json");
            break;
        case 'p':
            snprintf(filepath, sizeof(filepath), "subjects_json/phisik.json");
            break;
        case 'e':
            snprintf(filepath, sizeof(filepath), "subjects_json/english.json");
            break;
        case 'd':
            snprintf(filepath, sizeof(filepath), "subjects_json/german.json");
            break;
        case 'i':
            snprintf(filepath, sizeof(filepath), "subjects_json/informatik.json");
            break;
        case 'k':
            snprintf(filepath, sizeof(filepath), "subjects_json/kunst.json");
            break;
        case 'r':
            snprintf(filepath, sizeof(filepath), "subjects_json/religion.json");
            break;
        default:
            *count = 0;
            return NULL;
    }

    FILE *file = fopen(filepath, "r");
    if (!file) {
        *count = 0;
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = malloc(length + 1);
    fread(buffer, 1, length, file);
    buffer[length] = '\0';
    fclose(file);

    cJSON *json = cJSON_Parse(buffer);
    free(buffer);

    if (!json) {
        *count = 0;
        return NULL;
    }

    //get jsoin task array
    cJSON *tasks_array = cJSON_GetObjectItemCaseSensitive(json, "tasks");
    *count = cJSON_GetArraySize(tasks_array);

    Task *task_list = malloc(sizeof(Task) * (*count));

    for (int i = 0; i < *count; i++) {
        cJSON *item = cJSON_GetArrayItem(tasks_array, i);

        task_list[i].id = cJSON_GetObjectItemCaseSensitive(item, "id")->valueint;
        task_list[i].done = cJSON_GetObjectItemCaseSensitive(item, "done")->valueint;

        strncpy(task_list[i].date, cJSON_GetObjectItemCaseSensitive(item, "date")->valuestring, 11);
        strncpy(task_list[i].deadline, cJSON_GetObjectItemCaseSensitive(item, "deadline")->valuestring, 11);
        strncpy(task_list[i].subject, cJSON_GetObjectItemCaseSensitive(item, "subject")->valuestring, 32);
        strncpy(task_list[i].content, cJSON_GetObjectItemCaseSensitive(item, "content")->valuestring, 256);
    }

    cJSON_Delete(json);
    return task_list;
}

// Alle Fächer laden und zusammenführen
Task* get_all_tasks(int *total_count) {
    char subjects[] = {'m', 'p', 'e', 'd', 'i', 'k', 'r'};
    int num_subjects = sizeof(subjects) / sizeof(subjects[0]);

    Task *all_tasks = NULL;
    *total_count = 0;

    for (int i = 0; i < num_subjects; i++) {
        int sub_count = 0;
        Task *sub_tasks = get_tasks_for_subject(subjects[i], &sub_count);   //gets a whole task list from all and gets tge sub count increase via a pointer

        if (sub_tasks != NULL && sub_count > 0) {
            // Speicher erweitern für die neuen Tasks
            all_tasks = realloc(all_tasks, sizeof(Task) * (*total_count + sub_count));

            // Neue Tasks hinten anfügen
            memcpy(all_tasks + *total_count, sub_tasks, sizeof(Task) * sub_count);
            *total_count += sub_count;

            free(sub_tasks); // Temporäres Fach-Array freigeben
        }
    }

    return all_tasks;
}

void enable_input_echo(void) {
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag |= (ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
}

void disable_input_echo(void) {
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
}

void append_task(Task **all_tasks, int *total) {
    int new_index = *total;
    Task *tmp = realloc(*all_tasks, sizeof(Task) * (*total + 1));
    if (!tmp) return;
    *all_tasks = tmp;

    enable_input_echo();

    printf("subject: \n");
    fgets((*all_tasks)[new_index].subject, sizeof((*all_tasks)[new_index].subject), stdin);
    (*all_tasks)[new_index].subject[strcspn((*all_tasks)[new_index].subject, "\n")] = '\0';

    printf("date: \n");
    fgets((*all_tasks)[new_index].date, sizeof((*all_tasks)[new_index].date), stdin);
    (*all_tasks)[new_index].date[strcspn((*all_tasks)[new_index].date, "\n")] = '\0';

    printf("deadline: \n");
    fgets((*all_tasks)[new_index].deadline, sizeof((*all_tasks)[new_index].deadline), stdin);
    (*all_tasks)[new_index].deadline[strcspn((*all_tasks)[new_index].deadline, "\n")] = '\0';

    printf("content: \n");
    fgets((*all_tasks)[new_index].content, sizeof((*all_tasks)[new_index].content), stdin);
    (*all_tasks)[new_index].content[strcspn((*all_tasks)[new_index].content, "\n")] = '\0';

    disable_input_echo();

    (*all_tasks)[new_index].id = new_index;
    (*all_tasks)[new_index].done = 0;

    (*total)++;
}

void delete_task(Task *all_tasks, int *total, int del) {
    for (int i = del; i < *total - 1; i++) {
        all_tasks[i] = all_tasks[i + 1];
    }
    (*total)--;
}

void edit_task(Task **all_tasks, int *total, int edit_file) {
    enable_input_echo();

    char z[100];
    printf("edit (date/deadline/content): ");
    fgets(z, sizeof(z), stdin);
    z[strcspn(z, "\n")] = '\0';

    if (strcmp(z, "date") == 0) {
        printf("date: ");
        fgets((*all_tasks)[edit_file].date, sizeof((*all_tasks)[edit_file].date), stdin);
        (*all_tasks)[edit_file].date[strcspn((*all_tasks)[edit_file].date, "\n")] = '\0';
    }else if (strcmp(z, "deadline") == 0) {
        printf("deadline: ");
        fgets((*all_tasks)[edit_file].deadline, sizeof((*all_tasks)[edit_file].deadline), stdin);
        (*all_tasks)[edit_file].deadline[strcspn((*all_tasks)[edit_file].deadline, "\n")] = '\0';
    }else if (strcmp(z, "content") == 0) {
        printf("content: ");
        fgets((*all_tasks)[edit_file].content, sizeof((*all_tasks)[edit_file].content), stdin);
        (*all_tasks)[edit_file].content[strcspn((*all_tasks)[edit_file].content, "\n")] = '\0';
    }

    disable_input_echo();
}

void mark_as_x(Task *all_tasks, int *total, int edit_file) {
    all_tasks[edit_file].done = !all_tasks[edit_file].done;
}

// Sortierfunktionen für qsort
int cmp_subject(const void *a, const void *b) {
    return strcmp(((Task*)a)->subject, ((Task*)b)->subject);
}
int cmp_date(const void *a, const void *b) {
    return strcmp(((Task*)a)->date, ((Task*)b)->date);
}
int cmp_deadline(const void *a, const void *b) {
    return strcmp(((Task*)a)->deadline, ((Task*)b)->deadline);
}

int subject_matches_code(const char *subject, char code) {
    switch (code) {
        case 'm': return strcmp(subject, "mathe") == 0;
        case 'p': return strcmp(subject, "phisik") == 0;
        case 'e': return strcmp(subject, "englisch") == 0;
        case 'd': return strcmp(subject, "deutsch") == 0;
        case 'i': return strcmp(subject, "informatik") == 0;
        case 'k': return strcmp(subject, "kunst") == 0;
        case 'r': return strcmp(subject, "religion") == 0;
    }
    return 0;
}

// Schreibt alle Tasks gruppiert nach Fach zurück in die jeweilige subjects_json/*.json
void save_all_tasks(Task *all_tasks, int total) {
    char subjects[] = {'m', 'p', 'e', 'd', 'i', 'k', 'r'};
    int num_subjects = sizeof(subjects) / sizeof(subjects[0]);

    for (int s = 0; s < num_subjects; s++) {
        char filepath[64];
        switch (subjects[s]) {
            case 'm': snprintf(filepath, sizeof(filepath), "subjects_json/math.json"); break;
            case 'p': snprintf(filepath, sizeof(filepath), "subjects_json/phisik.json"); break;
            case 'e': snprintf(filepath, sizeof(filepath), "subjects_json/english.json"); break;
            case 'd': snprintf(filepath, sizeof(filepath), "subjects_json/german.json"); break;
            case 'i': snprintf(filepath, sizeof(filepath), "subjects_json/informatik.json"); break;
            case 'k': snprintf(filepath, sizeof(filepath), "subjects_json/kunst.json"); break;
            case 'r': snprintf(filepath, sizeof(filepath), "subjects_json/religion.json"); break;
        }

        cJSON *root = cJSON_CreateObject();
        cJSON *tasks_array = cJSON_CreateArray();
        cJSON_AddItemToObject(root, "tasks", tasks_array);

        for (int i = 0; i < total; i++) {
            if (subject_matches_code(all_tasks[i].subject, subjects[s])) {
                cJSON *item = cJSON_CreateObject();
                cJSON_AddNumberToObject(item, "id", all_tasks[i].id);
                cJSON_AddStringToObject(item, "date", all_tasks[i].date);
                cJSON_AddStringToObject(item, "deadline", all_tasks[i].deadline);
                cJSON_AddStringToObject(item, "subject", all_tasks[i].subject);
                cJSON_AddStringToObject(item, "content", all_tasks[i].content);
                cJSON_AddBoolToObject(item, "done", all_tasks[i].done);
                cJSON_AddItemToArray(tasks_array, item);
            }
        }

        char *json_str = cJSON_Print(root);
        FILE *f = fopen(filepath, "w");
        if (f) {
            fputs(json_str, f);
            fclose(f);
        }
        free(json_str);
        cJSON_Delete(root);
    }
}