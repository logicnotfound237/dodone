#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/dom/elements.hpp>
#include <vector>
#include <string>
#include <fstream>
#include <chrono> // For getting current time
#include <iomanip> // For std::put_time and std::get_time
#include "json.hpp" // Include the nlohmann/json header

using namespace ftxui;
using json = nlohmann::json;

// Renamed from Task to DodoneTask to avoid ambiguity with ftxui::Task
struct DodoneTask {
    std::string title;
    std::string date; // Now stores date and time
};

// --- JSON Serialization/Deserialization for DodoneTask ---
void to_json(json& j, const DodoneTask& p) {
    j = json{{"title", p.title}, {"date", p.date}}; // Include date in JSON
}

void from_json(const json& j, DodoneTask& p) {
    j.at("title").get_to(p.title);
    if (j.contains("date")) {
        j.at("date").get_to(p.date);
    } else {
        p.date = "";
    }
}
// --- End JSON Serialization/Deserialization ---

enum Column {
    TODO = 0,
    IN_PROGRESS = 1,
    DONE = 2,
    DATE_COLUMN = 3 // This column will now be "Finished On"
};

const int NUMBER_OF_TASK_COLUMNS = 3;
const int TOTAL_COLUMNS = 4;

// Renamed JSON file to dodone.json
const std::string DATABASE_FILE = "dodone.json";
const std::string PERMA_TASK_TITLE = "a/d/r/Entr"; // Define the special task title

// Function to save tasks to JSON
void saveTasksToJson(const std::vector<DodoneTask> columns[NUMBER_OF_TASK_COLUMNS], const std::string& filename) {
    json j_columns;
    for (int i = 0; i < NUMBER_OF_TASK_COLUMNS; ++i) {
        // If saving the TODO column, exclude the "perma" task from being written to JSON.
        // This ensures it's always initialized by the app and not duplicated by loading from file.
        if (i == TODO && !columns[TODO].empty() && columns[TODO][0].title == PERMA_TASK_TITLE) {
            std::vector<DodoneTask> temp_todo = columns[TODO];
            temp_todo.erase(temp_todo.begin()); // Remove "perma" for saving
            j_columns[i] = temp_todo;
        } else {
            j_columns[i] = columns[i];
        }
    }

    std::ofstream ofs(filename);
    if (ofs.is_open()) {
        ofs << j_columns.dump(4);
        ofs.close();
    } else {
        // Error handling
    }
}

// Function to load tasks from JSON
void loadTasksFromJson(std::vector<DodoneTask> columns[NUMBER_OF_TASK_COLUMNS], const std::string& filename) {
    std::ifstream ifs(filename);
    if (ifs.is_open()) {
        try {
            json j_columns;
            ifs >> j_columns;
            ifs.close();

            for (int i = 0; i < NUMBER_OF_TASK_COLUMNS; ++i) {
                if (j_columns.size() > i && j_columns[i].is_array()) {
                    columns[i] = j_columns[i].get<std::vector<DodoneTask>>();
                } else {
                    columns[i].clear();
                }
            }
        } catch (const json::parse_error& e) {
            // JSON parsing error
            for (int i = 0; i < NUMBER_OF_TASK_COLUMNS; ++i) {
                columns[i].clear();
            }
        }
    }
}

// Helper to get current date and time as string
std::string getCurrentDateTimeString() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm* ptm = std::localtime(&now_c);
    std::stringstream ss;
    // Format: YYYY-MM-DD HH:MM:SS
    ss << std::put_time(ptm, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}


int main() {
    std::vector<DodoneTask> columns[NUMBER_OF_TASK_COLUMNS]; // Renamed to DodoneTask

    loadTasksFromJson(columns, DATABASE_FILE);

    // After loading, ensure "perma" is at the top of the TODO list.
    // This handles cases where it's the first run, or if it was somehow
    // manually removed from the JSON file (though `saveTasksToJson` tries to prevent this).
    if (columns[TODO].empty() || columns[TODO][0].title != PERMA_TASK_TITLE) {
        // Insert perma at the beginning. If it exists elsewhere, it will be duplicate,
        // but its special status only applies to index 0.
        // A more robust solution might remove duplicates first.
        columns[TODO].insert(columns[TODO].begin(), {PERMA_TASK_TITLE, ""});
    }

    int selected_column = 0;
    int selected_index[TOTAL_COLUMNS] = {0, 0, 0, 0};
    bool grabbing = false;
    DodoneTask grabbed_task; // Renamed to DodoneTask

    bool adding = false;
    bool renaming = false;
    bool modifying_date = false;
    std::string input_buffer;

    auto screen = ScreenInteractive::TerminalOutput();

    auto render_column = [&](const std::string& name, const std::vector<DodoneTask>& tasks, int col_index, bool is_date_column = false) { // Renamed to DodoneTask
        Elements entries;
        int sel_index = selected_index[col_index];
        bool is_selected_col = (selected_column == col_index);

        for (size_t i = 0; i <= tasks.size(); ++i) {
            bool inserting_here = grabbing && is_selected_col && ((int)i == sel_index) && !is_date_column;

            if (inserting_here) {
                entries.push_back(text("[ " + grabbed_task.title + " ]") | bold | color(Color::Yellow));
            }

            if (i < tasks.size()) {
                bool is_selected_row_in_col = is_selected_col && ((int)i == sel_index);

                std::string display_text;
                if (is_date_column) {
                    display_text = tasks[i].date.empty() ? "(not set)" : tasks[i].date;
                } else {
                    display_text = tasks[i].title;
                }

                if ((renaming || modifying_date) && is_selected_row_in_col) {
                    entries.push_back(hbox({
                        text("> "),
                        text((modifying_date ? "Set Date/Time: " : "Renaming: ")),
                        text(input_buffer) | bold | color(Color::Yellow)
                    }));
                } else {
                    if (is_selected_row_in_col) {
                        // Highlight "perma" differently to show it's special
                        if (col_index == TODO && i == 0 && tasks[i].title == PERMA_TASK_TITLE) {
                            entries.push_back(text("> " + display_text) | bold | color(Color::Red)); // Unique color for perma
                        } else {
                            entries.push_back(text("> " + display_text) | bold | color(Color::Green));
                        }
                    } else {
                        entries.push_back(text("  " + display_text));
                    }
                }
            }
        }

        if (adding && is_selected_col && !is_date_column) {
            entries.push_back(hbox({
                text("+ Add: "),
                text(input_buffer) | bold | color(Color::Blue)
            }));
        }

        if (tasks.empty() && !(grabbing && is_selected_col && !is_date_column)) {
            entries.push_back(text("  (empty)") | dim);
        }

        Color header_color;
        if (col_index == TODO)             header_color = Color::SkyBlue1;
        else if (col_index == IN_PROGRESS) header_color = Color::Orange1;
        else if (col_index == DONE)        header_color = Color::MediumSpringGreen;
        else if (col_index == DATE_COLUMN) header_color = Color::Magenta3;

        return window(
            hbox({
                text("─ "),
                text(name) | color(header_color) | bold,
                text(" ─")
            }),
            vbox(std::move(entries))
        );
    };

    auto renderer = Renderer([&] {
        auto todo = render_column("TO DO", columns[TODO], TODO) | color(Color::White);
        auto in_progress = render_column("IN PROGRESS", columns[IN_PROGRESS], IN_PROGRESS) | color(Color::White);
        auto done = render_column("DONE", columns[DONE], DONE) | color(Color::White);
        auto finished_on = render_column("Finished On", columns[DONE], DATE_COLUMN, true) | color(Color::White);

        auto inner_row = hbox({
            todo,
            separatorHeavy(),
            in_progress,
            separatorHeavy(),
            done,
            separatorHeavy(),
            finished_on
        });

        return hbox({
            borderHeavy(inner_row) | color(Color::Purple)
        });
    });

    auto event_handler = CatchEvent(renderer, [&](Event event) {
        int& index = selected_index[selected_column];
        int current_column_task_count = (selected_column == DATE_COLUMN) ? columns[DONE].size() : columns[selected_column].size();
        int max_index = grabbing ? current_column_task_count : (current_column_task_count - 1);

        std::vector<DodoneTask>& current_tasks = (selected_column == DATE_COLUMN) ? columns[DONE] : columns[selected_column]; // Renamed to DodoneTask

        // --- Check for "perma" special case ---
        // If the current selection is "perma" in the TODO column, prevent actions.
        bool is_perma_task = (selected_column == TODO && index == 0 && !current_tasks.empty() && current_tasks[0].title == PERMA_TASK_TITLE);

        if (adding || renaming || modifying_date) {
            if (event.is_character()) {
                input_buffer += event.character();
                return true;
            }
            if (event == Event::Backspace && !input_buffer.empty()) {
                input_buffer.pop_back();
                return true;
            }
            if (event == Event::Return) {
                if (adding) {
                    // When adding in TODO, insert after perma if perma is present
                    current_tasks.insert(current_tasks.begin() + (selected_column == TODO ? 1 : index + 1), { input_buffer, "" });
                    index++;
                    adding = false;
                } else if (renaming) {
                    if (!current_tasks.empty() && !is_perma_task) { // Prevent renaming perma
                        current_tasks[index].title = input_buffer;
                    }
                    renaming = false;
                } else if (modifying_date) {
                    if (!current_tasks.empty() && !is_perma_task) { // Prevent modifying date for perma (though it has no date)
                        current_tasks[index].date = input_buffer;
                    }
                    modifying_date = false;
                }
                input_buffer.clear();
                saveTasksToJson(columns, DATABASE_FILE);
                return true;
            }
            if (event == Event::Escape) {
                adding = false;
                renaming = false;
                modifying_date = false;
                input_buffer.clear();
                return true;
            }
            return true;
        }

        // Adjust max_index for TODO column if perma is present, to prevent cursor from going below it when adding.
        // Also ensure cursor cannot go past perma in normal navigation if it's the only task.
        if (selected_column == TODO && !columns[TODO].empty() && columns[TODO][0].title == PERMA_TASK_TITLE) {
             if (event == Event::ArrowDown && index == 0 && columns[TODO].size() == 1) {
                return true; // Cannot move down if perma is the only task
            }
            if (event == Event::ArrowUp && index == 0) {
                return true; // Cannot move up from perma
            }
        }


        if (event == Event::ArrowDown && index < max_index) {
            index++;
            return true;
        }
        if (event == Event::ArrowUp && index > 0) {
            index--;
            return true;
        }
        if (event == Event::ArrowLeft && selected_column > 0) {
            selected_column--;
            // If perma is the only task in TODO, ensure cursor doesn't land on it if coming from right
            if (selected_column == TODO && columns[TODO].size() == 1 && columns[TODO][0].title == PERMA_TASK_TITLE) {
                selected_index[selected_column] = 0; // Ensure cursor is on perma
            } else if (selected_column == TODO && !columns[TODO].empty()) {
                selected_index[selected_column] = std::min(selected_index[selected_column], (int)columns[TODO].size() - 1);
            }
            return true;
        }
        if (event == Event::ArrowRight && selected_column < (TOTAL_COLUMNS - 1)) {
            selected_column++;
            // If moving to a new column, ensure index is valid for that column
            if (!current_tasks.empty() && selected_index[selected_column] >= (int)current_tasks.size() && selected_column != DATE_COLUMN) {
                selected_index[selected_column] = current_tasks.size() - 1;
            } else if (selected_column == DATE_COLUMN && !columns[DONE].empty() && selected_index[selected_column] >= (int)columns[DONE].size()) {
                 selected_index[selected_column] = columns[DONE].size() - 1;
            } else if (current_tasks.empty() && selected_column != DATE_COLUMN) {
                selected_index[selected_column] = 0; // If column is empty, set index to 0
            } else if (selected_column == DATE_COLUMN && columns[DONE].empty()) {
                selected_index[selected_column] = 0; // If DONE is empty, date column is empty
            }
            return true;
        }
        if (event == Event::Return) {
            if (selected_column == DATE_COLUMN) {
                return true; // Cannot grab/move from Date column
            }
            // Prevent grabbing "perma"
            if (!grabbing && is_perma_task) {
                return true;
            }

            if (!grabbing) {
                // GRABBING A TASK
                if (current_tasks.empty()) return true;

                // If the task is being grabbed from the DONE column, clear its date.
                // This prepares it for a new completion time if it returns to DONE.
                if (selected_column == DONE) {
                    grabbed_task = current_tasks[index];
                    grabbed_task.date = ""; // Clear the date when moving OUT of DONE
                } else {
                    grabbed_task = current_tasks[index];
                }

                current_tasks.erase(current_tasks.begin() + index);
                if (index >= (int)current_tasks.size() && index > 0) index--;
                // After removing, if current_tasks[0] is perma and it was not selected, ensure index is 0 or 1.
                // This scenario is complex if perma is not at 0 or if something else is at 0.
                // Simple logic: if the list becomes empty (except perma), index becomes 0.
                if (current_tasks.empty() && selected_column != TODO) {
                    index = 0;
                } else if (selected_column == TODO && current_tasks.size() == 1 && current_tasks[0].title == PERMA_TASK_TITLE) {
                    index = 0; // Keep cursor on perma if it's the only one left
                } else if (index >= (int)current_tasks.size()) {
                    index = (int)current_tasks.size() - 1;
                }


                grabbing = true;
            } else {
                // DROPPING A TASK
                // If dropping into DONE column, auto-set date/time if it's currently empty
                if (selected_column == DONE && grabbed_task.date.empty()) {
                    grabbed_task.date = getCurrentDateTimeString();
                }
                // When dropping into TODO, always insert after "perma" if "perma" is present.
                if (selected_column == TODO && !columns[TODO].empty() && columns[TODO][0].title == PERMA_TASK_TITLE) {
                    current_tasks.insert(current_tasks.begin() + std::max(1, index), grabbed_task);
                    if (index == 0) index = 1; // Move cursor after perma if it was at perma's pos
                } else {
                    current_tasks.insert(current_tasks.begin() + index, grabbed_task);
                }
                grabbing = false;
            }
            saveTasksToJson(columns, DATABASE_FILE);
            return true;
        }
        if (!grabbing && event == Event::Character('d') && selected_column != DATE_COLUMN) {
            if (!current_tasks.empty() && !is_perma_task) { // Prevent deleting perma
                current_tasks.erase(current_tasks.begin() + index);
                if (index >= (int)current_tasks.size() && index > 0) index--;
                saveTasksToJson(columns, DATABASE_FILE);
            }
            return true;
        }
        if (!grabbing && event == Event::Character('a') && selected_column != DATE_COLUMN) {
            input_buffer.clear();
            adding = true;
            return true;
        }
        if (!grabbing && event == Event::Character('r')) {
            if (!current_tasks.empty() && !is_perma_task) { // Prevent renaming/modifying date for perma
                if (selected_column == DATE_COLUMN) {
                    input_buffer = current_tasks[index].date;
                    modifying_date = true;
                } else {
                    input_buffer = current_tasks[index].title;
                    renaming = true;
                }
            }
            return true;
        }

        return false;
    });

    screen.Loop(event_handler);

    saveTasksToJson(columns, DATABASE_FILE);

    return 0;
}