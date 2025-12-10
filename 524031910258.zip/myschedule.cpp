#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>
#include <mutex>
#include <algorithm>
#include <cctype>
#include <functional>
#include <cstring>
#include <chrono>
#include <queue>
#include <condition_variable>
#include <windows.h> 

using namespace std;

string encryptPassword(const string& password) {
    string encrypted = password;
    char key = 0x55; 
    for (char& c : encrypted) {
        c ^= key;
    }
    return encrypted;
}

string decryptPassword(const string& encrypted) {
    return encryptPassword(encrypted); 
}

struct Task {
    int id;
    string name;
    time_t start_time;
    string priority; // "high", "medium", "low"
    string category; // "study", "entertainment", "life"
    time_t remind_time;
    bool reminded;
};

class UserManager {
private:
    unordered_map<string, string> users; // username -> encrypted_password
    const string userFile = "users.dat";
    mutex mtx;

public:
    UserManager() {
        loadUsers();
    }

    bool registerUser(const string& username, const string& password) {
        lock_guard<mutex> lock(mtx);
        if (users.find(username) != users.end()) return false;
        users[username] = encryptPassword(password);
        saveUsers();
        return true;
    }

    bool authenticate(const string& username, const string& password) {
        auto it = users.find(username);
        if (it == users.end()) return false;
        return password == decryptPassword(it->second);
    }

    void loadUsers() {
        ifstream file(userFile);
        if (file.is_open()) {
            string username, encrypted;
            while (file >> username >> encrypted) {
                users[username] = encrypted;
            }
        }
    }

    void saveUsers() {
        ofstream file(userFile);
        for (const auto& [user, enc] : users) {
            file << user << " " << enc << "\n";
        }
    }
};

class TaskManager {
private:
    vector<Task> tasks;
    string username;
    const string taskFile;
    int nextId = 1;
    mutex mtx;

public:
    TaskManager(const string& uname) : username(uname), 
        taskFile(uname + "_tasks.dat") {
        loadTasks();
    }

    bool addTask(Task task) {
        lock_guard<mutex> lock(mtx);
        
        for (const auto& t : tasks) {
            if (t.name == task.name && t.start_time == task.start_time) {
                return false;
            }
        }
        
        task.id = nextId++;
        tasks.push_back(task);
        saveTasks();
        return true;
    }

    bool deleteTask(int id) {
        lock_guard<mutex> lock(mtx);
        auto it = remove_if(tasks.begin(), tasks.end(), 
            [id](const Task& t) { return t.id == id; });
        
        if (it != tasks.end()) {
            tasks.erase(it, tasks.end());
            saveTasks();
            return true;
        }
        return false;
    }

    vector<Task> getTasksByDay(time_t day) {
        vector<Task> result;
        tm target_tm;
        localtime_s(&target_tm, &day);
        
        for (const auto& task : tasks) {
            tm task_tm;
            localtime_s(&task_tm, &task.start_time);
            if (task_tm.tm_year == target_tm.tm_year &&
                task_tm.tm_mon == target_tm.tm_mon &&
                task_tm.tm_mday == target_tm.tm_mday) {
                result.push_back(task);
            }
        }
        
        sort(result.begin(), result.end(), 
            [](const Task& a, const Task& b) {
                return a.start_time < b.start_time;
            });
        
        return result;
    }

    vector<Task> getAllTasks() {
        sort(tasks.begin(), tasks.end(), 
            [](const Task& a, const Task& b) {
                return a.start_time < b.start_time;
            });
        return tasks;
    }

    void loadTasks() {
        ifstream file(taskFile);
        if (file.is_open()) {
            Task task;
            while (file >> task.id >> task.name >> task.start_time 
                >> task.priority >> task.category >> task.remind_time >> task.reminded) {
                tasks.push_back(task);
                if (task.id >= nextId) nextId = task.id + 1;
            }
        }
    }

    void saveTasks() {
        ofstream file(taskFile);
        for (const auto& task : tasks) {
            file << task.id << " " << task.name << " " << task.start_time << " "
                 << task.priority << " " << task.category << " " 
                 << task.remind_time << " " << task.reminded << "\n";
        }
    }

    vector<Task>& getTaskList() { return tasks; }
};

class AudioPlayer {
public:
    static void playTone() {
        Beep(523, 500); 
    }
};

class ReminderSystem {
private:
    TaskManager& taskManager;
    thread reminderThread;
    bool running = false;
    condition_variable cv;
    mutex mtx;

    void checkReminders() {
        while (running) {
            {
                lock_guard<mutex> lock(mtx);
                auto now = time(nullptr);
                for (auto& task : taskManager.getTaskList()) {
                    if (!task.reminded && task.remind_time <= now) {
                        system("cls");
                        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
                        SetConsoleTextAttribute(hConsole, 12); 
                        cout << "========================================\n";
                        cout << "          !! TASK REMINDER !!\n";
                        cout << "========================================\n";
                        cout << " Task:    " << task.name << "\n";
                        cout << " Time:    " << ctime(&task.start_time);
                        cout << " Priority: " << task.priority << "\n";
                        cout << " Category: " << task.category << "\n";
                        cout << "========================================\n";
                        SetConsoleTextAttribute(hConsole, 7);

                        AudioPlayer::playTone(); 
                        
                        task.reminded = true;
                    }
                }
                taskManager.saveTasks();
            }
            this_thread::sleep_for(chrono::seconds(30)); 
        }
    }

public:
    ReminderSystem(TaskManager& tm) : taskManager(tm) {}

    void start() {
        running = true;
        reminderThread = thread(&ReminderSystem::checkReminders, this);
    }

    void stop() {
        running = false;
        if (reminderThread.joinable()) reminderThread.join();
    }
};

class CommandLineInterface {
private:
    UserManager userManager;
    unique_ptr<TaskManager> taskManager;
    unique_ptr<ReminderSystem> reminderSystem;
    string currentUser;

    time_t parseTime(const string& timeStr) {
        tm tm = {};
        istringstream ss(timeStr);
        ss >> get_time(&tm, "%Y-%m-%d %H:%M");
        if (ss.fail()) {
            istringstream ss2(timeStr);
            tm = {};
            ss2 >> get_time(&tm, "%Y-%m-%d");
            if (ss2.fail()) {
                throw runtime_error("Invalid time format: '" + timeStr + "'");
            }
            tm.tm_hour = 12;
            tm.tm_min = 0;
        }
        return mktime(&tm);
    }

    string timeToString(time_t t) {
        tm tm;
        localtime_s(&tm, &t);
        char buffer[20];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", &tm);
        return string(buffer);
    }

public:
    void printHelp() {
        cout << "MySchedule - Command Line Task Manager\n"
             << "Usage:\n"
             << "  myschedule run\n"
             << "  myschedule <username> <password> <command> [args...]\n\n"
             << "Commands:\n"
             << "  register <username> <password> - Register new user\n"
             << "  login <username> <password> - Login to existing account\n"
             << "  addtask <name> <date> [time] [priority] [category] [remind_offset_min]\n"
             << "      date format: YYYY-MM-DD (required)\n"
             << "      time format: HH:MM (optional, default: 12:00)\n"
             << "      priority: high/medium/low (default: medium)\n"
             << "      category: study/entertainment/life (default: life)\n"
             << "      remind_offset_min: minutes before start (default: 15)\n"
             << "  showtasks [day <date>] - Show all tasks or tasks for a specific day\n"
             << "      date format: YYYY-MM-DD\n"
             << "  deltask <id> - Delete task by ID\n"
             << "  help - Show this help message\n"
             << "  exit - Exit the program\n";
    }

    void displayTasks(const vector<Task>& tasks) {
        if (tasks.empty()) {
            cout << "No tasks found.\n";
            return;
        }

        cout << "ID  Name                Start Time        Priority  Category\n";
        cout << "------------------------------------------------------------\n";
        for (const auto& task : tasks) {
            cout << left << setw(3) << task.id 
                 << setw(20) << (task.name.length() > 18 ? task.name.substr(0, 15) + "..." : task.name)
                 << setw(18) << timeToString(task.start_time)
                 << setw(10) << task.priority
                 << task.category << "\n";
        }
    }

    void runShell() {
        string command;
        cout << "MySchedule Shell. Type 'help' for commands.\n";
        
        while (true) {
            if (currentUser.empty()) {
                cout << "Not logged in> ";
            } else {
                cout << currentUser << "> ";
            }
            
            getline(cin, command);
            if (command.empty()) continue;
            
            vector<string> args;
            stringstream ss(command);
            string arg;
            while (ss >> arg) args.push_back(arg);
            
            processCommand(args);
        }
    }

    void processCommand(vector<string> args) {
        if (args.empty()) return;
        
        string cmd = args[0];
        
        if (cmd == "exit") {
            if (reminderSystem) reminderSystem->stop();
            exit(0);
        }
        else if (cmd == "help") {
            printHelp();
        }
        else if (cmd == "register") {
            if (args.size() < 3) {
                cout << "Usage: register <username> <password>\n";
                return;
            }
            if (userManager.registerUser(args[1], args[2])) {
                cout << "User registered successfully.\n";
            } else {
                cout << "Username already exists.\n";
            }
        }
        else if (cmd == "login") {
            if (args.size() < 3) {
                cout << "Usage: login <username> <password>\n";
                return;
            }
            if (userManager.authenticate(args[1], args[2])) {
                currentUser = args[1];
                taskManager = make_unique<TaskManager>(currentUser);
                reminderSystem = make_unique<ReminderSystem>(*taskManager);
                reminderSystem->start();
                cout << "Logged in successfully.\n";
            } else {
                cout << "Invalid credentials.\n";
            }
        }
        else if (cmd == "addtask") {
            if (!taskManager) {
                cout << "Please login first.\n";
                return;
            }
            if (args.size() < 3) {
                cout << "Usage: addtask <name> <date> [time] [priority] [category] [remind_offset_min]\n";
                cout << "Example: addtask Meeting 2023-12-15 14:00 high study 30\n";
                return;
            }
            
            Task task;
            task.name = args[1];
            
            try {
                // 日期是必需的
                string dateStr = args[2];
                string timeStr = "12:00"; // 默认时间
                
                // 检查下一个参数是否是时间
                if (args.size() > 3) {
                    if (args[3].find(':') != string::npos) {
                        timeStr = args[3];
                        // 移除时间参数
                        args.erase(args.begin() + 3);
                    }
                }
                
                string datetimeStr = dateStr + " " + timeStr;
                task.start_time = parseTime(datetimeStr);
            } catch (const exception& e) {
                cout << "Error: " << e.what() << "\n";
                return;
            }
            
            task.priority = "medium";
            task.category = "life";
            task.remind_time = task.start_time - 15 * 60; 
            task.reminded = false;
            
            for (size_t i = 3; i < args.size(); i++) {
                if (args[i] == "high" || args[i] == "medium" || args[i] == "low") {
                    task.priority = args[i];
                }
                else if (args[i] == "study" || args[i] == "entertainment" || args[i] == "life") {
                    task.category = args[i];
                }
                else {
                    try {
                        int offset = stoi(args[i]);
                        task.remind_time = task.start_time - offset * 60;
                    } catch (...) {
                        cout << "Invalid parameter: " << args[i] << "\n";
                        return;
                    }
                }
            }
            
            if (taskManager->addTask(task)) {
                cout << "Task added successfully." <<"\n";
            } else {
                cout << "Failed to add task. Task with same name and time already exists.\n";
            }
        }
        else if (cmd == "showtasks") {
            if (!taskManager) {
                cout << "Please login first.\n";
                return;
            }
            
            if (args.size() > 1 && args[1] == "day") {
                if (args.size() < 3) {
                    cout << "Usage: showtasks day <date>\n";
                    cout << "Example: showtasks day 2023-12-15\n";
                    return;
                }
                try {
                    time_t day = parseTime(args[2] + " 00:00");
                    auto tasks = taskManager->getTasksByDay(day);
                    displayTasks(tasks);
                } catch (const exception& e) {
                    cout << "Error: " << e.what() << "\n";
                }
            } else {
                auto tasks = taskManager->getAllTasks();
                displayTasks(tasks);
            }
        }
        else if (cmd == "deltask") {
            if (!taskManager) {
                cout << "Please login first.\n";
                return;
            }
            if (args.size() < 2) {
                cout << "Usage: deltask <id>\n";
                return;
            }
            try {
                int id = stoi(args[1]);
                if (taskManager->deleteTask(id)) {
                    cout << "Task deleted successfully.\n";
                } else {
                    cout << "Task not found.\n";
                }
            } catch (...) {
                cout << "Invalid task ID.\n";
            }
        }
        else {
            cout << "Unknown command: " << cmd << ". Type 'help' for available commands.\n";
        }
    }

    void executeSingleCommand(int argc, char* argv[]) {
        if (argc < 4) {
            printHelp();
            return;
        }
        
        string username = argv[1];
        string password = argv[2];
        string command = argv[3];
        
        if (command == "register") {
            if (userManager.registerUser(username, password)) {
                cout << "User registered successfully.\n";
            } else {
                cout << "Username already exists.\n";
            }
            return;
        }
        
        if (!userManager.authenticate(username, password)) {
            cout << "Authentication failed.\n";
            return;
        }
        
        currentUser = username;
        taskManager = make_unique<TaskManager>(currentUser);
        reminderSystem = make_unique<ReminderSystem>(*taskManager);
        reminderSystem->start();
        
        vector<string> args;
        for (int i = 3; i < argc; i++) {
            args.push_back(argv[i]);
        }
        
        processCommand(args);
        
        this_thread::sleep_for(chrono::seconds(1));
        if (reminderSystem) reminderSystem->stop();
    }
};

int main(int argc, char* argv[]) {
    CommandLineInterface cli;
    
    if (argc == 2 && string(argv[1]) == "run") {
        cli.runShell();
    }
    else if (argc >= 4) {
        cli.executeSingleCommand(argc, argv);
    }
    else {
        cli.printHelp();
    }
    
    return 0;
}