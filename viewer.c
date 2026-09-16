

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#define API_URL "http://127.0.0.1:4301"
#define BUFFER_SIZE 4096

static void print_icon(void);

static void read_line(const char *prompt, char *buffer, size_t size) {
    printf("%s", prompt);
    if (fgets(buffer, (int)size, stdin) == NULL) {
        buffer[0] = '\0';
        return;
    }
    buffer[strcspn(buffer, "\n")] = '\0';
}

#ifndef _WIN32
static struct termios original_terminal;

static void set_raw_mode(int enabled) {
    struct termios terminal;

    if (enabled) {
        tcgetattr(STDIN_FILENO, &original_terminal);
        terminal = original_terminal;
        terminal.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal);
    } else {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_terminal);
    }
}
#endif

static int read_menu_key(void) {
#ifdef _WIN32
    int key = _getch();
    if (key == 0 || key == 224) return 256 + _getch();
    return key;
#else
    int key = getchar();
    if (key == 27 && getchar() == '[') {
        key = getchar();
        if (key == 'A') return 1001;
        if (key == 'B') return 1002;
    }
    return key;
#endif
}

static int select_menu(const char *title, const char **options, int count) {
    int selected = 0;
    int key;

#ifndef _WIN32
    set_raw_mode(1);
#endif
    for (;;) {
        printf("\033[2J\033[H");
        if (strcmp(title, "marg ghost") == 0) {
            print_icon();
            printf("\n");
        } else {
            printf("%s\n\n", title);
        }
        for (int index = 0; index < count; index++) {
            if (index == selected)
                printf("\033[7m> %s\033[0m\n", options[index]);
            else
                printf("  %s\n", options[index]);
        }
        fflush(stdout);

        key = read_menu_key();
        if (key == 1001 || (key == 224 + 72))
            selected = (selected - 1 + count) % count;
        else if (key == 1002 || (key == 224 + 80))
            selected = (selected + 1) % count;
        else if (key == '\r' || key == '\n')
            break;
        else if (key == 'q' || key == 'Q') {
#ifndef _WIN32
            set_raw_mode(0);
#endif
            return -1;
        }
    }
#ifndef _WIN32
    set_raw_mode(0);
#endif
    return selected;
}

static void wait_for_enter(void) {
    char buffer[8];
    read_line("\nEnter zum Fortfahren...", buffer, sizeof(buffer));
}

static void print_icon(void) {
    FILE *file = fopen("assets/icon.txt", "r");
    char line[256];

    if (file == NULL) return;
    while (fgets(line, sizeof(line), file) != NULL) fputs(line, stdout);
    fclose(file);
}

static void request(const char *method, const char *path, const char *body) {
    char command[BUFFER_SIZE];
    const char *body_file = "api_body.json";
    int result;

    if (body == NULL) {
        snprintf(command, sizeof(command), "curl -sS -X %s %s%s", method, API_URL, path);
    } else {
        FILE *file = fopen(body_file, "w");
        if (file == NULL) {
            fprintf(stderr, "JSON-Datei konnte nicht erstellt werden.\n");
            return;
        }
        fputs(body, file);
        fclose(file);
        snprintf(
            command,
            sizeof(command),
            "curl -sS -X %s -H \"Content-Type: application/json\" --data-binary @%s %s%s",
            method, body_file, API_URL, path
        );
    }

    result = system(command);
    if (body != NULL) remove(body_file);
    if (result != 0) {
        fprintf(stderr, "\nServeranfrage fehlgeschlagen. Läuft uvicorn?\n");
    }
    printf("\n");
}

static void show_tables(void) {
    request("GET", "/tables", NULL);
}

static void show_table(const char *table) {
    char sort_by[64], direction[8], path[192];

    read_line("Sortieren nach (leer = Standardsortierung): ", sort_by, sizeof(sort_by));
    if (sort_by[0] == '\0') {
        snprintf(path, sizeof(path), "/tables/%s/view", table);
    } else {
        read_line("Absteigend? (j/n): ", direction, sizeof(direction));
        snprintf(
            path, sizeof(path), "/tables/%s/view?sort_by=%s&desc=%s",
            table, sort_by, (direction[0] == 'j' || direction[0] == 'J') ? "true" : "false"
        );
    }
    request("GET", path, NULL);
}

static void add_homework(void) {
    char id[64], subject[128], date[32], deadline[32], content[256], done[16];
    char body[BUFFER_SIZE];

    read_line("ID: ", id, sizeof(id));
    read_line("Fach: ", subject, sizeof(subject));
    read_line("Datum: ", date, sizeof(date));
    read_line("Deadline: ", deadline, sizeof(deadline));
    read_line("Inhalt: ", content, sizeof(content));
    read_line("Erledigt (0/1): ", done, sizeof(done));
    snprintf(
        body, sizeof(body),
        "{\"id\":\"%s\",\"subject\":\"%s\",\"date\":\"%s\",\"deadline\":\"%s\",\"content\":\"%s\",\"done\":\"%s\"}",
        id, subject, date, deadline, content, done
    );
    request("POST", "/tables/homework", body);
}

static void add_test(void) {
    char id[64], subject[128], date[32], content[256], impact[64];
    char body[BUFFER_SIZE];

    read_line("ID: ", id, sizeof(id));
    read_line("Fach: ", subject, sizeof(subject));
    read_line("Datum: ", date, sizeof(date));
    read_line("Inhalt: ", content, sizeof(content));
    read_line("Auswirkung: ", impact, sizeof(impact));
    snprintf(
        body, sizeof(body),
        "{\"id\":\"%s\",\"subject\":\"%s\",\"date\":\"%s\",\"content\":\"%s\",\"impact\":\"%s\"}",
        id, subject, date, content, impact
    );
    request("POST", "/tables/test", body);
}

static void add_grade(void) {
    char subject[128], oral[32], written[32];
    char body[BUFFER_SIZE];

    read_line("Fach: ", subject, sizeof(subject));
    read_line("Mündlich: ", oral, sizeof(oral));
    read_line("Schriftlich: ", written, sizeof(written));
    snprintf(
        body, sizeof(body),
        "{\"subject\":\"%s\",\"oral\":\"%s\",\"written\":\"%s\"}",
        subject, oral, written
    );
    request("POST", "/tables/grades", body);
}

static void grade_test(void) {
    char id[64], grade[32], path[128], body[64];

    read_line("Test-ID: ", id, sizeof(id));
    read_line("Testnote: ", grade, sizeof(grade));
    snprintf(path, sizeof(path), "/tests/%s/grade", id);
    snprintf(body, sizeof(body), "{\"grade\":\"%s\"}", grade);
    request("POST", path, body);
}

static void update_row(const char *table) {
    char id[64], column[64], value[256], path[128], body[512];

    read_line("ID: ", id, sizeof(id));
    read_line("Spalte (subject/date/content/impact/done): ", column, sizeof(column));
    read_line("Neuer Wert: ", value, sizeof(value));
    snprintf(path, sizeof(path), "/tables/%s/%s", table, id);
    snprintf(body, sizeof(body), "{\"%s\":\"%s\"}", column, value);
    request("PATCH", path, body);
}

static void delete_row(const char *table) {
    char id[64], path[128];

    read_line("ID: ", id, sizeof(id));
    snprintf(path, sizeof(path), "/tables/%s/%s", table, id);
    request("DELETE", path, NULL);
}

static void run_homework_menu(void) {
    const char *options[] = {
        "Homework anzeigen/sortieren", "Homework anlegen", "Homework bearbeiten",
        "Homework löschen", "Zurück"
    };
    int choice;

    for (;;) {
        choice = select_menu("Homework", options, 5);
        if (choice < 0 || choice == 4) return;
        if (choice == 0) show_table("homework");
        else if (choice == 1) add_homework();
        else if (choice == 2) update_row("homework");
        else if (choice == 3) delete_row("homework");
        wait_for_enter();
    }
}

static void run_test_menu(void) {
    const char *options[] = {
        "Tests anzeigen/sortieren", "Test anlegen", "Testnote eintragen",
        "Test bearbeiten", "Test löschen", "Zurück"
    };
    int choice;

    for (;;) {
        choice = select_menu("Tests", options, 6);
        if (choice < 0 || choice == 5) return;
        if (choice == 0) show_table("test");
        else if (choice == 1) add_test();
        else if (choice == 2) grade_test();
        else if (choice == 3) update_row("test");
        else if (choice == 4) delete_row("test");
        wait_for_enter();
    }
}

static void run_grade_menu(void) {
    const char *options[] = {"Noten anzeigen/sortieren", "Note anlegen", "Zurück"};
    int choice;

    for (;;) {
        choice = select_menu("Grades", options, 3);
        if (choice < 0 || choice == 2) return;
        if (choice == 0) show_table("grades");
        else if (choice == 1) add_grade();
        wait_for_enter();
    }
}

int main(void) {
    const char *options[] = {
        "Homework",
        "Tests",
        "Grades",
        "Beenden"
    };
    int choice;

    for (;;) {
        choice = select_menu("marg ghost", options, 4);
        if (choice < 0 || choice == 3) break;
        if (choice == 0) run_homework_menu();
        else if (choice == 1) run_test_menu();
        else if (choice == 2) run_grade_menu();
    }

    return 0;
}

