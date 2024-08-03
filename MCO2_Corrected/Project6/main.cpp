class BufferPrint;
class Process;
class ReadyQueue;
class CoreCPU;
class MemoryBlock;
class MemoryAllocator;
struct SchedulerConfig;
struct SchedulerState;

// Include necessary headers
#include <iostream>
#include <unordered_map>
#include <functional>
#include <string>
#include <sstream>
#include <fstream> 
#include <thread>
#include <queue>
#include <map>
#include <conio.h>
#include <condition_variable>
#include <Windows.h>
#include <vector>
#include <algorithm>
#include <mutex>
#include <iomanip>
#include <atomic>
#include <random>


// Constants
static int MARQUEE_SCREEN = -99;
static int MAIN_SCREEN = 0;
const int NOT_ALLOCATED = -1;
const int INACTIVE = -2;
static bool IS_DEBUG_MODE = true;

// Function declarations
std::string format_time(const std::chrono::system_clock::time_point& tp);
SchedulerConfig* initialize();
void printSchedulerConfig(const SchedulerConfig* config, BufferPrint* bp, int rolled);
void marquee();

// Class and struct declarations
class BufferPrint {
private:
    HANDLE hConsole;
    COORD bufferSize;
    SMALL_RECT windowSize;
    std::map<int, std::vector<std::string>> persistentLines;
    std::map<int, std::vector<std::string>> nonPersistentLines;
    std::map<int, std::vector<std::string>> lastRenderedLines;
    std::string currentInput;
    int currentScreen;
    std::mutex mutex;

    std::thread inputThread;
    std::thread renderThread;

    std::queue<char> inputQueue;
    std::mutex inputQueueMutex;
    std::condition_variable inputQueueCV;

    std::atomic<bool> running;

    std::atomic<bool> paused;
    std::condition_variable pauseCV;
    std::mutex pauseMutex;

    void writeLineAt(const std::string& line, SHORT y);
    void inputThreadFunc();
    void renderThreadFunc();

public:
    BufferPrint();
    ~BufferPrint();
    std::string getLineInput(const std::string& prompt = "");
    void print(const std::string& text, bool isPersistent = false);
    void debug(const std::string& text, bool isPersistent = false);
    void switchScreen(int newScreen);
    void pause();
    void resume();
};

class Process {
public:
    int id;
    int print_count;
    int completed_count;
    int pages; //used
    int memory;
    bool isBackingStored;
    std::queue<std::string> print_commands;
    std::string latest_time;
    std::string name;
    std::mutex ioMutex;

    Process(int id, int print_count, int memory);
    Process(int id, int print_count, int memory, int page_count);
    int getProcessMemoryReq();
    void storeToBackingStore(BufferPrint* printer);
    void returnFromBackingStore(BufferPrint* printer);
};

class ReadyQueue {
private:
    std::mutex mutex;
public:
    std::queue<Process*> process_queue;
    std::map<std::string, Process*> unfinished_processes;
    std::map<int, Process*> running_processes;
    std::vector<Process*> finished_processes;

    void add_to_process_queue(Process* process);
    void return_to_process_queue(Process* process);
    bool is_process_queue_empty();
    Process* on_process_running();
    void back_to_queue();
    Process* shortest_process_first(bool preemptive, Process* current_process = nullptr);
    void on_finish_process(int id);

    Process* getProcessFromIDAndPop(int id);
    std::string getProcessNameFromID(int id);
};

struct SchedulerConfig {
    int numCPU;
    std::string schedulerType;
    int quantumCycles;
    bool preemptive;
    double batchProcessFreq;
    int minIns;
    int maxIns;
    double delaysPerExec;
    int maxMemApp;
    int minMemProc;
    int maxMemProc;
    int minPageProc;
    int maxPageProc;
};

struct SchedulerState {
public:
    std::vector<CoreCPU*> cpus;

    bool is_scheduler_test_running;
    bool is_screen_s_running;

    int generation = 0;
    int pagePerProcess;
    int memoryPerProcessRolled;

    ReadyQueue ready_queue;
    MemoryAllocator* allocator = nullptr;

    //std::vector<Process*> backing_store;
    std::unique_ptr<std::vector<Process*>> backing_store = std::make_unique<std::vector<Process*>>();
    std::unique_ptr<std::vector<int>> usedAddresses = std::make_unique<std::vector<int>>();

    std::condition_variable cv;
    std::mutex pushMutex;
    std::mutex resizeMutex;

    void moveRandomProcessToBackingStore(CoreCPU* except, SchedulerConfig* config, SchedulerState* state, BufferPrint* printer);

    void resizeUsedAddresses(int size);
    int getUsedAddress(int index);
    void setUsedAddress(int index, int value);

    void pushProcessToBackingStore(Process* process);
    bool isBackingStoreEmpty();
    int getBackingStoreSize();

    int getRandomUsedAddressIndex(BufferPrint* printer);

    Process* getProcessFromBackingStore(BufferPrint* printer);
    Process* getSpecificProcessFromBackingStore(int id, BufferPrint* printer);
};

class MemoryBlock {
public:
    int address;
    int size;
    MemoryBlock(int address, int size);
};

class MemoryAllocator {
private:
    int totalMemorySize;
public:
    std::mutex mutex;
    BufferPrint* printer;
    MemoryAllocator(BufferPrint* printer, int totalSize);
    virtual int getTotalUsedMemory() = 0;
    int getTotalMemorySize();
    virtual int allocate(SchedulerConfig* config, SchedulerState* state, BufferPrint* bp, Process* process) = 0;
    virtual void deallocate(int address) = 0;
    ~MemoryAllocator();
};

class FlatMemoryAllocator : virtual public MemoryAllocator {
public:
    std::vector<MemoryBlock*> memory;
    int findFirstFit(int size);
    int getTotalUsedMemory() override;
    int allocate(SchedulerConfig* config, SchedulerState* state, BufferPrint* bp, Process* process) override;
    void deallocate(int address) override;
    MemoryBlock* getMemoryBlockFromAddress(int address, bool lock);
    FlatMemoryAllocator(BufferPrint* printer, int totalSize);
    ~FlatMemoryAllocator();
};

class PagingMemoryAllocator :virtual public MemoryAllocator {
private:
    int framesAvailable; // tracker, should sync with the list...
    int totalFrames;
    int pagesPerProcess;

    int useFrames(int id, int frames);

public:
    int pageSize;
    std::unique_ptr<std::map<int, int>> usedFrames = std::make_unique<std::map<int, int>>();
    int getTotalUsedMemory() override;
    int allocate(SchedulerConfig* config, SchedulerState* state, BufferPrint* bp, Process* process) override;
    void deallocate(int address) override;
    int getRemainingFrames();
    int getTotalFrames();

    PagingMemoryAllocator(BufferPrint* printer, int totalSize, int pagesPerProcess, int procMemory);
};

class CoreCPU {
public:
    std::mutex mutex;
    std::mutex backstoreMutex;
    std::mutex printMutex;
    std::string name;
    bool isBackingStoreLocked = false;
    int idleTicks;
    int activeTicks;

    //int currentlyUsedAddress = -1;

    Process* current_process;

    void process_realtime(BufferPrint* printer, SchedulerState* state, SchedulerConfig* config);
    CoreCPU(std::string name, BufferPrint* p, SchedulerState* s, SchedulerConfig* c);
};

int main() {
    BufferPrint* buffered_printer = new BufferPrint();
    SchedulerConfig* config = nullptr;
    SchedulerState* state = nullptr;

    std::unordered_map<std::string, std::function<void(SchedulerConfig* config, SchedulerState* state, BufferPrint* bp)>> commands;

    auto command_screen_r = [](SchedulerConfig* config, SchedulerState* state, BufferPrint* bp, std::string command) {
        Process* process = state->ready_queue.unfinished_processes[command];

        bp->switchScreen(process->id + 1);

        std::string screenCommand;
        while (true) {
            bp->print("Process: " + process->name);
            bp->print("Id: " + std::to_string(process->id));
            bp->print("Process print count: " + std::to_string(process->completed_count) + "/" + std::to_string(process->print_count));
            bp->print("");

            screenCommand = bp->getLineInput("Enter a command: ");

            if (screenCommand == "exit") {
                bp->switchScreen(MAIN_SCREEN);
                break;
            }
            else if (screenCommand == "process-smi") {
                bp->print("");
                continue;
            }
            else {
                bp->print("Invalid command", true);
            }
        }
        };

    commands["debug"] = [](SchedulerConfig* config, SchedulerState* state, BufferPrint* bp) {

        IS_DEBUG_MODE = !IS_DEBUG_MODE;

        };

    commands["vmstat"] = [](SchedulerConfig* config, SchedulerState* state, BufferPrint* bp) {

        {
            //std::lock_guard<std::mutex> lock(state->resizeMutex);

            bool isFlatAllocator = config->minPageProc == 1 && config->maxPageProc == 1;
            int size = state->generation + 1 < state->usedAddresses->size() ? state->generation + 1 : state->usedAddresses->size();

            bp->print("");
            bp->print("");
            bp->print("---------------------------------------------");
            bp->print("-----------------VM Status-------------------");
            bp->print("---------------------------------------------");
            bp->print(std::to_string(config->maxMemApp) + " = Total Overall Memory");

            if (isFlatAllocator) {
                auto func = [state]() {
                    std::lock_guard<std::mutex> lock(state->allocator->mutex);
                    FlatMemoryAllocator* allocator = dynamic_cast<FlatMemoryAllocator*>(state->allocator);
                    int usedMemory = 0;
                    int lastEndAddress = 0;

                    for (MemoryBlock* block : allocator->memory) {
                        // Add the gap between this block and the previous one
                        if (block->address > lastEndAddress) {
                            usedMemory += block->address - lastEndAddress;
                        }

                        // Add the size of the current block
                        usedMemory += block->size;

                        // Update the last end address
                        lastEndAddress = block->address + block->size;
                    }

                    return usedMemory;

                    };

                bp->print(std::to_string(func()) + " = Total Used Memory");
                bp->print(std::to_string(state->allocator->getTotalUsedMemory()) + " = Active Memory");
                bp->print(std::to_string(config->maxMemApp - state->allocator->getTotalUsedMemory()) + " = Inactive Memory");
            }

            else {
                PagingMemoryAllocator* allocator = dynamic_cast<PagingMemoryAllocator*>(state->allocator);
                size_t total = allocator->getTotalUsedMemory() * allocator->pageSize;
                bp->print(std::to_string(total) + " = Total Used Memory");
                //External not possible in paging.
                bp->print(std::to_string(total) + " = Active Used Memory");
                bp->print(std::to_string(config->maxMemApp - total) + " = Inactive Used Memory");
            }


            int inactiveTicks = 0;
            int activeTicks = 0;

            for (CoreCPU* cores : state->cpus) {
                std::lock_guard<std::mutex> printLock(cores->printMutex);
                inactiveTicks += cores->idleTicks;
                activeTicks += cores->activeTicks;
            }

            bp->print(std::to_string(inactiveTicks) + " = Idle CPU Ticks");
            bp->print(std::to_string(activeTicks) + " = Active CPU Ticks");
            bp->print(std::to_string(inactiveTicks + activeTicks) + " = Total CPU Ticks");
            bp->print("---------------------------------------------");
            bp->print("---------------------------------------------");
            bp->print("");
        }

        };

    commands["process-smi"] = [](SchedulerConfig* config, SchedulerState* state, BufferPrint* bp) {
        if (!config || !state || !bp) {
            bp->print("Something has gone wrong with config and/or state and/or bp");
            return;
        }
        bp->print("");
        bp->print("");

        int cpu_running_counter = 0;


        bool isFlatAllocator = config->minPageProc == 1 && config->maxPageProc == 1;




        bp->print("---------------------------------------------");
        bp->print("---------------------------------------------");
        //bp->print(std::to_string(config->minPageProc) + " = Min, " + std::to_string(config->maxPageProc)  + " = Max");

        for (const auto cpu : state->cpus) {
            std::lock_guard<std::mutex> lock(cpu->printMutex);
            if (cpu->current_process != nullptr) {
                cpu_running_counter++;
            }
        }
        double perc = static_cast<double>(cpu_running_counter) / config->numCPU;
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << (perc * 100) << "%";
        bp->print("CPU Utilization: " + ss.str());

        if (!isFlatAllocator) {
            PagingMemoryAllocator* allocator = dynamic_cast<PagingMemoryAllocator*>(state->allocator);

            bp->print("Frames Total: " + std::to_string(allocator->getTotalFrames()));
            bp->print("Frames Available: " + std::to_string(allocator->getRemainingFrames()));
            bp->print("Frames Used: " + std::to_string(allocator->getTotalFrames() - allocator->getRemainingFrames()));
        }

        else {
            bp->print("Memory Available: " + std::to_string(state->allocator->getTotalMemorySize() - state->allocator->getTotalUsedMemory()));
            bp->print("Memory Used: " + std::to_string(state->allocator->getTotalUsedMemory()));
        }

        bp->print("---------------------------------------------");
        bp->print("Running processes and memory usage");
        bp->print("---------------------------------------------");

        if (isFlatAllocator) {
            FlatMemoryAllocator* allocator = dynamic_cast<FlatMemoryAllocator*>(state->allocator);
            //bp->print("Test...");

            {
                std::lock_guard<std::mutex> lock(allocator->mutex);
                for (int i = 0; i < state->usedAddresses->size(); i++) {

                    int address = state->getUsedAddress(i);

                    if (address != NOT_ALLOCATED) {
                        MemoryBlock* block = allocator->getMemoryBlockFromAddress(address, false);
                        std::string processName = state->ready_queue.getProcessNameFromID(i);
                        int memory = block->size;
                        int address = block->address;
                        bp->print(processName + " occupies " + std::to_string(address) + " address with size " + std::to_string(memory));
                    }
                }
            }
        }

        else {
            PagingMemoryAllocator* allocator = dynamic_cast<PagingMemoryAllocator*>(state->allocator);

            {
                std::lock_guard<std::mutex> lock(allocator->mutex);
                for (const auto& pair : *allocator->usedFrames) {
                    // Access key and value here
                    std::string processName = state->ready_queue.getProcessNameFromID(pair.first);
                    int allocatedFrames = pair.second;
                    bp->print(processName + " currently uses " + std::to_string(allocatedFrames) + " frames.");

                }
            }

        }

        bp->print("");
        bp->print("---------------------------------------------");
        bp->print("---------------------------------------------");

        bp->print("");
        bp->print("");

        };

    commands["screen-ls"] = [](SchedulerConfig* config, SchedulerState* state, BufferPrint* bp) {
        if (!config || !state || !bp) {
            bp->print("Something has gone wrong with config and/or state and/or bp");
            return;
        }

        bp->print("");
        bp->print("");

        int cpu_running_counter = 0;
        int memory = 0;

        for (const auto cpu : state->cpus) {
            std::lock_guard<std::mutex> lock(cpu->printMutex);
            if (cpu->current_process != nullptr) {
                cpu_running_counter++;
            }
        }



        /*{
            std::lock_guard<std::mutex> lock(state->resizeMutex);
            for (int i = 0; i < state->usedAddresses->size(); i++) {
                bp->print("Process ID[" + std::to_string(i) + "] -> Address it uses = " + std::to_string((*state->usedAddresses)[i]));
            }
        }*/

        double perc = static_cast<double>(cpu_running_counter) / config->numCPU;
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << (perc * 100) << "%";
        bp->print("CPU Utilization: " + ss.str());
        bp->print("Cores Used: " + std::to_string(cpu_running_counter));
        bp->print("Cores Available: " + std::to_string(config->numCPU - cpu_running_counter));
        bp->print("");
        bp->print("---------------------------------------------");
        bp->print("Running processes:");

        for (const auto cpu : state->cpus) {
            std::unique_lock<std::mutex> lock(cpu->printMutex);
            if (cpu->current_process != nullptr && cpu->current_process->id != INACTIVE) {
                std::string time = cpu->current_process->latest_time;
                bp->print(cpu->current_process->name + "     " + time + "     Core: " + cpu->name + " " + std::to_string(cpu->current_process->completed_count) + " / " + std::to_string(cpu->current_process->print_count));
            }
        }

        bp->print("");
        bp->print("Finished processes:");

        for (const auto processes : state->ready_queue.finished_processes) {
            std::string time = processes->latest_time;
            bp->print(processes->name + "     " + time + "     Finished " + std::to_string(processes->completed_count) + " / " + std::to_string(processes->print_count));
        }
        bp->print("---------------------------------------------");
        bp->print("");
        bp->print("");
        };

    commands["marquee"] = [](SchedulerConfig* config, SchedulerState* state, BufferPrint* bp) {
        bp->pause();
        bp->switchScreen(MARQUEE_SCREEN);
        marquee();
        bp->resume();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        bp->switchScreen(MAIN_SCREEN);
        };

    commands["scheduler-test"] = [](SchedulerConfig* config, SchedulerState* state, BufferPrint* bp) {
        if (!config || !state)
            return;

        if (state->is_scheduler_test_running) {
            bp->print("Scheduler-Test is already ongoing...", true);
            return;
        }

        state->is_scheduler_test_running = true;

        while (state->is_scheduler_test_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(config->batchProcessFreq * 1000)));
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> printCount(config->minIns, config->maxIns);

            Process* process = new Process(state->generation++, printCount(gen), state->memoryPerProcessRolled, state->pagePerProcess);
            state->ready_queue.add_to_process_queue(process);
            state->cv.notify_one();
            bp->debug("Added " + process->name + " with " + std::to_string(state->memoryPerProcessRolled) + " memory requirements");
        }
        };

    commands["scheduler-stop"] = [](SchedulerConfig* config, SchedulerState* state, BufferPrint* bp) {
        if (state) {
            if (state->is_scheduler_test_running) {
                state->is_scheduler_test_running = false;
            }
            else {
                bp->print("Scheduler-Test is already stopped...", true);
            }
        }
        };

    commands["report-util"] = [](SchedulerConfig* config, SchedulerState* state, BufferPrint* bp) {
        if (!config || !state || !bp) {
            std::ofstream outFile("csopesy-log.txt", std::ios::app);
            outFile << "Something has gone wrong with config and/or state and/or bp\n";
            outFile.close();
            return;
        }

        std::ofstream outFile("csopesy-log.txt", std::ios::app);

        int cpu_running_counter = 0;
        for (const auto cpu : state->cpus) {
            std::lock_guard<std::mutex> lock(cpu->mutex);
            if (cpu->current_process != nullptr) {
                cpu_running_counter++;
            }
        }
        double perc = static_cast<double>(cpu_running_counter) / config->numCPU;
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << (perc * 100) << "%";
        outFile << "CPU Utilization: " << ss.str() << "\n";
        outFile << "Cores Used: " << cpu_running_counter << "\n";
        outFile << "Cores Available: " << (config->numCPU - cpu_running_counter) << "\n";
        outFile << "\n";
        outFile << "---------------------------------------------\n";
        outFile << "Running processes:\n";
        for (const auto cpu : state->cpus) {
            std::unique_lock<std::mutex> lock(cpu->mutex);
            if (cpu->current_process != nullptr) {
                std::string time = cpu->current_process->latest_time;
                outFile << cpu->current_process->name << "     " << time << "     Core: " << cpu->name << " "
                    << cpu->current_process->completed_count << " / " << cpu->current_process->print_count << "\n";
            }
            lock.unlock();
        }
        outFile << "\n";
        outFile << "Finished processes:\n";
        for (const auto processes : state->ready_queue.finished_processes) {
            std::string time = processes->latest_time;
            outFile << processes->name << "     " << time << "     Finished "
                << processes->completed_count << " / " << processes->print_count << "\n";
        }
        outFile << "---------------------------------------------\n";
        outFile << "\n\n";

        outFile.close();
        };

    std::string command = "";

    while (true) {
        command = buffered_printer->getLineInput("Enter initialize to start: ");
        if (command == "exit") {
            buffered_printer->print("Exiting program...", true);
            return 0;
        }
        else if (command != "initialize") {
            buffered_printer->print("Command unrecognized. Please try again", true);
        }
        else {
            config = initialize();
            state = new SchedulerState();

            std::random_device rd;
            std::mt19937 gen(rd());

            int minExpPagePossible = static_cast<int>(std::ceil(std::log2(config->minPageProc)));
            int maxExpPagePossible = static_cast<int>(std::ceil(std::log2(config->maxPageProc)));

            int minExpMemPossible = static_cast<int>(std::ceil(std::log2(config->minMemProc)));
            int maxExpMemPossible = static_cast<int>(std::ceil(std::log2(config->maxMemProc)));

            std::uniform_int_distribution<int> disMem(minExpMemPossible, maxExpMemPossible);
            std::uniform_int_distribution<int> disPage(minExpPagePossible, maxExpPagePossible);

            state->memoryPerProcessRolled = static_cast<int>(pow(2, disMem(gen)));
            state->pagePerProcess = static_cast<int>(pow(2, disPage(gen)));

            if (config->minPageProc == 1 && config->maxPageProc == 1)
                state->allocator = new FlatMemoryAllocator(buffered_printer, config->maxMemApp);

            else {
                state->allocator = new PagingMemoryAllocator(buffered_printer, config->maxMemApp, state->pagePerProcess, state->memoryPerProcessRolled);
            }

            for (int i = 0; i < config->numCPU; i++) {
                CoreCPU* cpu = new CoreCPU("CPU_" + std::to_string(i), buffered_printer, state, config);
                state->cpus.push_back(cpu);
            }

            break;
        }
    }

    printSchedulerConfig(config, buffered_printer, config->minPageProc == 1 && config->maxPageProc == 1 ? -1 : state->pagePerProcess);

    if (!state || !config) {
        buffered_printer->print("Program unable to initialize state and/or config!", true);
        return -2;
    }

    while (command != "exit") {
        while (state->is_screen_s_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        command = "";
        command = buffered_printer->getLineInput("Enter a command: ");

        if (command.find("make8") != std::string::npos) {
            Process* process;
            for (int i = 0; i < 2; i++) {
                if (i % 2 == 0) {
                    process = new Process(state->generation++, 4, 64, 16);
                }
                else {
                    process = new Process(state->generation++, 4, 32, 16);
                }

                state->ready_queue.add_to_process_queue(process);
                state->cv.notify_one();
            }
        }

        if (command.find("screen -s ") != std::string::npos) {
            command = command.substr(10, command.size());
            if (state->ready_queue.unfinished_processes.count(command) > 0) {
                buffered_printer->print("Process already exists!", true);
                continue;
            }
            else {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> printCount(config->minIns, config->maxIns);

                Process* process = new Process(state->generation++, printCount(gen), state->memoryPerProcessRolled, state->pagePerProcess);

                //Process* process = new Process(state->generation++,
                //    rand() % (config->maxIns - config->minIns) + config->minIns,
                //    rand() % (config->maxMemProc - config->minMemProc) + config->minMemProc,
                //    rand() % (config->maxPageProc - config->minPageProc) + config->minPageProc);

                process->name = command;
                state->ready_queue.add_to_process_queue(process);
                state->cv.notify_one();
                command = "screen -r " + command;
            }
        }


        if (command.find("screen -r ") != std::string::npos) {
            command = command.substr(10, command.size());
            if (state->ready_queue.unfinished_processes.count(command) > 0) {
                command_screen_r(config, state, buffered_printer, command);
            }
            else {
                buffered_printer->print("Process " + command + " not found.", true);
            }
        }
        else if (commands.find(command) == commands.end()) {
            buffered_printer->print("Invalid command.", true);
        }
        else {
            auto function = commands[command];
            std::thread execute([function](SchedulerConfig* config, SchedulerState* state, BufferPrint* bp) {
                function(config, state, bp);
                }, config, state, buffered_printer);
            execute.detach();
        }
    }

    buffered_printer->print("Exiting program...", true);

    delete config;
    delete state;
    delete buffered_printer;

    return 0;
}

std::string format_time(const std::chrono::system_clock::time_point& tp) {
    std::time_t now_c = std::chrono::system_clock::to_time_t(tp);

    std::tm now_tm;

#ifdef _WIN32
    if (localtime_s(&now_tm, &now_c) != 0) {
        throw std::runtime_error("Failed to convert time using localtime_s");
    }
#else
    std::tm* local_tm_ptr = std::localtime(&now_c);
    if (!local_tm_ptr) {
        throw std::runtime_error("Failed to convert time using localtime");
    }
    now_tm = *local_tm_ptr;
#endif

    std::stringstream ss;
    ss << std::put_time(&now_tm, "%m/%d/%Y %H:%M:%S");
    return ss.str();
}

SchedulerConfig* initialize() {
    SchedulerConfig* config = new SchedulerConfig();
    std::ifstream file("config.txt");

    if (!file.is_open()) {
        std::cerr << "Error opening config.txt" << std::endl;
        exit(1);
    }

    file >> config->numCPU;
    file >> config->schedulerType;
    file >> config->quantumCycles;
    int preemptiveInt;
    file >> preemptiveInt;
    config->preemptive = (preemptiveInt == 1);
    file >> config->batchProcessFreq;
    file >> config->minIns;
    file >> config->maxIns;
    file >> config->delaysPerExec;
    file >> config->maxMemApp;
    file >> config->minMemProc;
    file >> config->maxMemProc;
    file >> config->minPageProc;
    file >> config->maxPageProc;

    file.close();
    return config;
}

void printSchedulerConfig(const SchedulerConfig* config, BufferPrint* bp, int rolled) {
    if (config == nullptr) {
        bp->print("SchedulerConfig is null");
        return;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3) << config->batchProcessFreq;

    bp->print("CSOPESY Simulator");
    bp->print("===================================");
    bp->print("Scheduler Configuration");
    bp->print("Number of CPUs: " + std::to_string(config->numCPU));
    bp->print("Scheduler Type: " + config->schedulerType);
    bp->print("Quantum Cycles: " + std::to_string(config->quantumCycles));
    bp->print("Preemptive: " + std::string(config->preemptive ? "Yes" : "No"));
    bp->print("Batch Process Frequency: " + oss.str() + " seconds");
    bp->print("Minimum Instructions: " + std::to_string(config->minIns));
    bp->print("Maximum Instructions: " + std::to_string(config->maxIns));
    oss.str("");
    oss.clear();
    oss << std::fixed << std::setprecision(3) << config->delaysPerExec;
    bp->print("Delays per Execution: " + oss.str() + " seconds");

    bp->print("Maximum Memory Available: " + std::to_string(config->maxMemApp));
    bp->print("Minimum Memory Per Process: " + std::to_string(config->minMemProc));
    bp->print("Maximum Memory Per Process: " + std::to_string(config->maxMemProc));
    bp->print("Minimum Page Per Process: " + std::to_string(config->minPageProc));
    bp->print("Maximum Page Per Process: " + std::to_string(config->maxPageProc));
    bp->print("Rolled Page/Process: " + std::to_string(rolled));

    bp->print("===================================");
    bp->print("");
}

// BufferPrint implementations
BufferPrint::BufferPrint() : currentScreen(0), running(true), paused(false) {
    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    bufferSize = csbi.dwSize;
    windowSize = csbi.srWindow;

    inputThread = std::thread(&BufferPrint::inputThreadFunc, this);
    renderThread = std::thread(&BufferPrint::renderThreadFunc, this);
}

BufferPrint::~BufferPrint() {
    running = false;
    if (inputThread.joinable()) inputThread.join();
    if (renderThread.joinable()) renderThread.join();
}

void BufferPrint::writeLineAt(const std::string& line, SHORT y) {
    COORD pos = { 0, y };
    SetConsoleCursorPosition(hConsole, pos);
    DWORD written;
    WriteConsoleA(hConsole, line.c_str(), static_cast<DWORD>(line.length()), &written, nullptr);

    // Clear the rest of the line
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    DWORD cellsToWrite = csbi.dwSize.X - written;
    FillConsoleOutputCharacterA(hConsole, ' ', cellsToWrite, { static_cast<SHORT>(written), y }, &written);
}

void BufferPrint::inputThreadFunc() {
    while (running) {
        {
            std::unique_lock<std::mutex> lock(pauseMutex);
            pauseCV.wait(lock, [this] { return !paused || !running; });
        }
        if (!running) break;

        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        int ch = _getch();
        if (ch == EOF) continue;

        std::lock_guard<std::mutex> lock(inputQueueMutex);
        inputQueue.push(static_cast<char>(ch));
        inputQueueCV.notify_one();
    }
}

void BufferPrint::renderThreadFunc() {
    while (running) {
        {
            std::unique_lock<std::mutex> lock(pauseMutex);
            pauseCV.wait(lock, [this] { return !paused || !running; });
        }
        if (!running) break;

        {
            std::lock_guard<std::mutex> lock(mutex);
            auto& currentNonPersistentLines = nonPersistentLines[currentScreen];
            auto& currentPersistentLines = persistentLines[currentScreen];
            auto& lastRendered = lastRenderedLines[currentScreen];

            for (size_t i = 0; i < currentNonPersistentLines.size(); ++i) {
                if (i >= lastRendered.size() || currentNonPersistentLines[i] != lastRendered[i]) {
                    writeLineAt(currentNonPersistentLines[i], static_cast<SHORT>(i));
                }
            }

            size_t startY = currentNonPersistentLines.size();
            for (size_t i = 0; i < currentPersistentLines.size(); ++i) {
                size_t index = startY + i;
                if (index >= lastRendered.size() || currentPersistentLines[i] != lastRendered[index]) {
                    writeLineAt(currentPersistentLines[i], static_cast<SHORT>(index));
                }
            }

            size_t inputY = startY + currentPersistentLines.size();
            if (inputY >= lastRendered.size() || currentInput != lastRendered[inputY]) {
                writeLineAt(currentInput, static_cast<SHORT>(inputY));
            }

            lastRendered = currentNonPersistentLines;
            lastRendered.insert(lastRendered.end(), currentPersistentLines.begin(), currentPersistentLines.end());
            lastRendered.push_back(currentInput);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }
}

std::string BufferPrint::getLineInput(const std::string& prompt) {
    std::string input;
    {
        std::lock_guard<std::mutex> lock(mutex);
        currentInput = prompt;
    }

    while (true) {
        std::unique_lock<std::mutex> lock(inputQueueMutex);
        inputQueueCV.wait(lock, [this] { return !inputQueue.empty() || !running; });

        if (!running) break;

        char ch = inputQueue.front();
        inputQueue.pop();
        lock.unlock();

        if (ch == '\r') break;

        if (ch == '\b') {
            if (!input.empty()) {
                input.pop_back();
                std::lock_guard<std::mutex> inputLock(mutex);
                currentInput = prompt + input;
            }
        }
        else {
            input += ch;
            std::lock_guard<std::mutex> inputLock(mutex);
            currentInput = prompt + input;
        }
    }

    {
        std::lock_guard<std::mutex> lock(mutex);
        persistentLines[currentScreen].push_back(currentInput);
        currentInput.clear();
    }

    return input;
}

void BufferPrint::print(const std::string& text, bool isPersistent) {
    std::lock_guard<std::mutex> lock(mutex);
    if (isPersistent) {
        persistentLines[currentScreen].push_back(text);
    }
    else {
        nonPersistentLines[currentScreen].push_back(text);
    }
}

void BufferPrint::debug(const std::string& text, bool isPersistent) {
    if (IS_DEBUG_MODE)
        this->print(text, isPersistent);
}

void BufferPrint::switchScreen(int newScreen) {
    std::lock_guard<std::mutex> lock(mutex);
    if (newScreen != currentScreen) {
        currentScreen = newScreen;
        currentInput.clear();
        if (persistentLines.find(currentScreen) == persistentLines.end()) {
            persistentLines[currentScreen] = std::vector<std::string>();
            nonPersistentLines[currentScreen] = std::vector<std::string>();
        }
        lastRenderedLines[currentScreen].clear();
        SetConsoleCursorPosition(hConsole, { 0, 0 });

        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hConsole, &csbi);
        DWORD written;
        FillConsoleOutputCharacter(hConsole, ' ', csbi.dwSize.X * csbi.dwSize.Y, { 0, 0 }, &written);
    }
}

void BufferPrint::pause() {
    std::lock_guard<std::mutex> lock(pauseMutex);
    paused = true;
}

void BufferPrint::resume() {
    {
        std::lock_guard<std::mutex> lock(pauseMutex);
        paused = false;
    }
    pauseCV.notify_all();
}

// Process implementations
Process::Process(int id, int print_count, int memory) : id(id), print_count(print_count), memory(memory), completed_count(0), pages(0) {
    name = "P" + std::to_string(id);
    for (int i = 0; i < print_count; ++i) {
        print_commands.push("Hello world from process " + std::to_string(id) + "!");
    }
}

Process::Process(int id, int print_count, int memory, int pages) : id(id), print_count(print_count), memory(memory), completed_count(0), pages(pages) {
    name = "P" + std::to_string(id);
    for (int i = 0; i < print_count; ++i) {
        print_commands.push("Hello world from process " + std::to_string(id) + "!");
    }
}

int Process::getProcessMemoryReq() {
    return this->memory;
}

void Process::storeToBackingStore(BufferPrint* printer) {
    std::lock_guard<std::mutex> lockIO(ioMutex);
    std::ofstream file(name + ".txt");
    isBackingStored = true;
    if (file.is_open()) {
        file << id << std::endl;
        file << pages << std::endl;
        file << print_count << std::endl;
        file << completed_count << std::endl;
        file << latest_time << std::endl;

        int queue_size = print_commands.size();
        file << queue_size << std::endl;

        std::queue<std::string> temp = print_commands;
        while (!temp.empty()) {
            file << temp.front() << std::endl;
            temp.pop();
        }

        file.close();
    }
    else {
        printer->print("[Error]: Unable to open file for writing: " + name + ".txt");
    }
}

void Process::returnFromBackingStore(BufferPrint* printer) {
    std::lock_guard<std::mutex> lockIO(ioMutex);
    std::ifstream file(name + ".txt");
    isBackingStored = false;
    if (file.is_open()) {
        file >> id;
        file >> pages;
        file >> print_count;
        file >> completed_count;
        file.ignore();
        std::getline(file, latest_time);
        int queue_size;
        file >> queue_size;
        file.ignore();
        std::queue<std::string> empty;
        std::swap(print_commands, empty);
        for (int i = 0; i < queue_size; ++i) {
            std::string command;
            std::getline(file, command);
            print_commands.push(command);
        }
        file.close();

        // Delete the file after reading
        if (std::remove((name + ".txt").c_str()) != 0) {
            printer->print("[Error]: Unable to delete file: " + name + ".txt");
        }
    }
    else {
        printer->print("[Error]: Unable to open file for reading: " + name + ".txt");
    }
}

// ReadyQueue implementations
void ReadyQueue::add_to_process_queue(Process* process) {
    std::lock_guard<std::mutex> lock(mutex);
    process_queue.push(process);
    unfinished_processes[process->name] = process;

}

void ReadyQueue::return_to_process_queue(Process* process) {
    std::lock_guard<std::mutex> lock(mutex);
    running_processes.erase(process->id);
    process_queue.push(process);
}

Process* ReadyQueue::on_process_running() {
    std::lock_guard<std::mutex> lock(mutex);
    if (process_queue.empty()) {
        return nullptr;
    }
    Process* process = process_queue.front();
    process_queue.pop();
    running_processes[process->id] = process;
    return process;
}

void ReadyQueue::back_to_queue() {
    std::lock_guard<std::mutex> lock(mutex);
    if (process_queue.empty()) {
        return;
    }
    Process* process = process_queue.front();
    process_queue.pop();
    process_queue.push(process);
}

bool ReadyQueue::is_process_queue_empty() {
    std::lock_guard<std::mutex> lock(mutex);
    return this->process_queue.empty();
}

Process* ReadyQueue::shortest_process_first(bool preemptive, Process* current_process) {
    std::lock_guard<std::mutex> lock(mutex);

    if (process_queue.empty()) {
        return current_process;
    }

    Process* shortest = nullptr;
    int shortest_remaining = INT_MAX;
    std::queue<Process*> temp_queue;

    while (!process_queue.empty()) {
        Process* temp = process_queue.front();
        process_queue.pop();

        int remaining = temp->print_count - temp->completed_count;
        if (remaining < shortest_remaining) {
            shortest = temp;
            shortest_remaining = remaining;
        }

        temp_queue.push(temp);
    }

    process_queue = temp_queue;

    if (current_process == nullptr || (preemptive && shortest_remaining < (current_process->print_count - current_process->completed_count))) {
        if (current_process) {
            process_queue.push(current_process);
            running_processes.erase(current_process->id);
        }

        std::queue<Process*> new_queue;
        while (!process_queue.empty()) {
            Process* temp = process_queue.front();
            process_queue.pop();
            if (temp != shortest) {
                new_queue.push(temp);
            }
        }
        process_queue = new_queue;

        running_processes[shortest->id] = shortest;
        return shortest;
    }

    return current_process;
}

void ReadyQueue::on_finish_process(int id) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = running_processes.find(id);
    if (it != running_processes.end()) {
        Process* process = it->second;
        running_processes.erase(it);
        unfinished_processes.erase(process->name);
        finished_processes.push_back(process);
    }
}

Process* ReadyQueue::getProcessFromIDAndPop(int id) {
    std::lock_guard<std::mutex> lock(mutex);

    if (process_queue.empty()) {
        return nullptr;
    }

    std::queue<Process*> temp_queue;
    Process* found_process = nullptr;

    while (!process_queue.empty()) {
        Process* temp = process_queue.front();
        process_queue.pop();

        if (temp->id == id) {
            found_process = temp;

        }

        //temp_queue.push(temp);

        else {
            temp_queue.push(temp);
        }
    }

    process_queue = temp_queue;

    return found_process;
}

std::string ReadyQueue::getProcessNameFromID(int id) {
    std::lock_guard<std::mutex> lock(mutex);

    if (process_queue.empty()) {
        return nullptr;
    }

    for (auto pair : this->unfinished_processes) {
        if (pair.second->id == id) {
            return pair.second->name;
        }
    }

    for (auto pair : this->finished_processes) {
        if (pair->id == id) {
            return pair->name;
        }
    }

    return "NULL";
}
// MemoryBlock implementation
MemoryBlock::MemoryBlock(int address, int size) : address(address), size(size) {}
// MemoryAllocator implementations
MemoryAllocator::MemoryAllocator(BufferPrint* printer, int totalSize) : printer(printer), totalMemorySize(totalSize) {}

MemoryAllocator::~MemoryAllocator() {

}

FlatMemoryAllocator::~FlatMemoryAllocator() {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto block : memory) {
        delete block;
    }
}

FlatMemoryAllocator::FlatMemoryAllocator(BufferPrint* printer, int totalSize) : MemoryAllocator(printer, totalSize) {

}

PagingMemoryAllocator::PagingMemoryAllocator(BufferPrint* printer, int totalSize, int pagesPerProcess, int procMemory) : MemoryAllocator(printer, totalSize), pagesPerProcess(pagesPerProcess) {

    //int pageSize = totalSize / pagesPerProcess; //64 / 16
    //this->framesAvailable = totalSize / pageSize; //1024 / 32 = 32 OR 64/4 = 16
    //this->totalFrames = framesAvailable;

    this->pageSize = ceil(static_cast<double>(procMemory) / static_cast<double>(pagesPerProcess)); // This calculates the size of each page

    // Calculate the number of frames available based on total memory and page size
    this->framesAvailable = totalSize / pageSize; // Total frames available in the system
    this->totalFrames = framesAvailable;

    printer->debug(std::to_string(pageSize) + " = Page Size");
    printer->debug(std::to_string(this->totalFrames) + " = Frames for OS available");


}

int MemoryAllocator::getTotalMemorySize() {
    return this->totalMemorySize;
}

int FlatMemoryAllocator::getTotalUsedMemory() {
    std::lock_guard<std::mutex> lock(mutex);
    int remaining = 0;
    for (MemoryBlock* block : memory) {
        remaining += block->size;
    }
    return remaining;
}

int FlatMemoryAllocator::findFirstFit(int size) {
    if (this->getTotalMemorySize() <= 0)
        return NOT_ALLOCATED;
    if (memory.empty()) {
        return size <= this->getTotalMemorySize() ? 0 : NOT_ALLOCATED;;
    }
    if (memory[0]->address >= size) {
        return 0;
    }
    for (size_t i = 0; i < memory.size() - 1; i++) {
        int endOfCurrent = memory[i]->address + memory[i]->size;
        int startOfNext = memory[i + 1]->address;
        if (startOfNext - endOfCurrent >= size) {
            return endOfCurrent;
        }
    }
    int endOfLast = memory.back()->address + memory.back()->size;
    if (this->getTotalMemorySize() - endOfLast >= size) {
        return endOfLast;
    }
    return NOT_ALLOCATED;
}

int FlatMemoryAllocator::allocate(SchedulerConfig* config, SchedulerState* state, BufferPrint* bp, Process* process) {
    std::lock_guard<std::mutex> lock(mutex);

    if (!process)
        return NOT_ALLOCATED;

    int start = findFirstFit(process->memory);
    if (start != NOT_ALLOCATED) {
        bp->debug("Allocated at " + std::to_string(start) + " with size " + std::to_string(process->memory));
        memory.push_back(new MemoryBlock(start, process->memory));
        std::sort(memory.begin(), memory.end(),
            [](const MemoryBlock* a, const MemoryBlock* b) {
                return a->address < b->address;
            });
        return start;
    }

    return NOT_ALLOCATED;
}

void FlatMemoryAllocator::deallocate(int address) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = std::find_if(memory.begin(), memory.end(),
        [address](const MemoryBlock* block) { return block->address == address; });
    if (it != memory.end()) {
        //printer->debug("Deallocated at " + std::to_string((*it)->address) + " with size " + std::to_string((*it)->size));
        delete* it;
        memory.erase(it);
        return;
    }

    throw std::runtime_error("Unable to locate " + std::to_string(address));
}

MemoryBlock* FlatMemoryAllocator::getMemoryBlockFromAddress(int address, bool lock) {
    if (lock)
        std::lock_guard<std::mutex> lock(mutex);

    auto it = std::find_if(memory.begin(), memory.end(),
        [address](const MemoryBlock* block) { return block->address == address; });
    if (it != memory.end()) {
        return *it;
    }

    return nullptr;
}

int PagingMemoryAllocator::allocate(SchedulerConfig* config, SchedulerState* state, BufferPrint* bp, Process* process) {
    std::lock_guard<std::mutex> lock(mutex);

    //printer->debug("P Memory requires: " + std::to_string(process->memory) + " and has " + std::to_string(process->pages) + " pages");


    return this->useFrames(process->id, ceil(process->getProcessMemoryReq() / this->pageSize));
}

int PagingMemoryAllocator::getTotalUsedMemory() {
    std::lock_guard<std::mutex> lock(mutex);
    return this->totalFrames - this->framesAvailable;
}

void PagingMemoryAllocator::deallocate(int id) {
    if (id == NOT_ALLOCATED) {
        throw std::runtime_error("Process is not allocated!");
    }
    std::lock_guard<std::mutex> lock(mutex);

    auto it = this->usedFrames->find(id);
    if (it != this->usedFrames->end()) {
        this->framesAvailable += it->second;
        printer->debug("Paging Allocator: Deallocated frame = " + std::to_string(it->second));
        this->usedFrames->erase(it);
    }
    else {
        throw std::runtime_error("Process ID not found in used frames!");
    }
}

int PagingMemoryAllocator::getTotalFrames() {
    return this->totalFrames;
}

//Frames can never be zero or lower.
//ID can not be null
int PagingMemoryAllocator::useFrames(int id, int frames) {

    auto it = this->usedFrames->find(id);
    int existingFrames = (it != this->usedFrames->end()) ? it->second : 0;

    int remainingFrames = frames - existingFrames; //If already allocated, this is the frame needed...

    if (this->totalFrames < frames) {
        printer->debug("[Memory Insufficient] Paging Allocator: Process Memory > OS Memory For ID " + std::to_string(id));
        return NOT_ALLOCATED;
    }


    if (this->framesAvailable == 0) {
        printer->debug("[Memory Full] Paging Allocator: Failed Allocation for ID " + std::to_string(id));
        return NOT_ALLOCATED;
    }



    if (remainingFrames <= 0) {
        // No additional frames needed
        printer->debug("Paging Allocator: Already Allocated!");
        return id;
    }

    printer->debug(std::to_string(id) + " ID wants to use " + std::to_string(frames));

    //If still cannot able to finish the remaining frames.
    if (this->framesAvailable < remainingFrames) {
        // Scenario 1: Partial allocation due to insufficient frames
        int partialAllocatedFrames = this->framesAvailable;
        this->framesAvailable = 0;
        (*this->usedFrames)[id] = existingFrames + partialAllocatedFrames;
        printer->debug("[Partial] Paging Allocator: Allocated " + std::to_string(partialAllocatedFrames) +
            " out of " + std::to_string(remainingFrames) + " for ID " + std::to_string(id));
        return NOT_ALLOCATED;
    }

    else if (existingFrames > 0) {
        // Scenario 2: Completing a partial allocation
        this->framesAvailable -= remainingFrames;
        (*this->usedFrames)[id] = frames;
        printer->debug("[Filling] Paging Allocator: Completed allocation for ID " + std::to_string(id) +
            ", added " + std::to_string(remainingFrames) + " frames");
        return id;
    }
    else {
        // Scenario 3: Full allocation from the start
        this->framesAvailable -= frames;
        (*this->usedFrames)[id] = frames;
        printer->debug("[Full] Paging Allocator: Fully allocated " + std::to_string(frames) +
            " frames for ID " + std::to_string(id));
        return id;
    }
}
int PagingMemoryAllocator::getRemainingFrames() {
    std::lock_guard<std::mutex> lock(mutex);
    return this->framesAvailable;
}

// CoreCPU implementations
void CoreCPU::process_realtime(BufferPrint* printer, SchedulerState* state, SchedulerConfig* config) {

    //bool isConditionalActive = true;

    if (config->schedulerType == "fcfs") {
        int addressAlloc = -1;
        while (true) {
            current_process = nullptr;
            {
                std::unique_lock<std::mutex> lock(mutex);
                isBackingStoreLocked = false;

                state->cv.wait(lock, [state]() {
                    //printer->print(this->name + ": " + std::to_string(!state->ready_queue.process_queue.empty() || !state->backing_store.empty()));
                    return !state->ready_queue.is_process_queue_empty() || !state->isBackingStoreEmpty();
                    });

                if (!current_process && state->ready_queue.is_process_queue_empty() && !state->isBackingStoreEmpty()) {
                    current_process = state->getProcessFromBackingStore(printer);
                }

                else {
                    current_process = state->ready_queue.on_process_running();
                    if (current_process) {
                        state->resizeUsedAddresses(current_process->id);
                    }
                }

                //If process is not null, and not yet allocated.
                if (current_process && (state->getUsedAddress(current_process->id) == NOT_ALLOCATED)) {
                    //printer->print(this->name + ": Allocating for " + current_process->name);
                    state->setUsedAddress(current_process->id, state->allocator->allocate(config, state, printer, current_process));

                    //First attempt
                    if (state->getUsedAddress(current_process->id) == NOT_ALLOCATED) {
                        //Second attempt
                        if (state->cpus.size() > 1) {
                            // Create a list of eligible CPUs (all except the current CPU)
                            std::vector<CoreCPU*> eligibleCPUs;
                            eligibleCPUs.reserve(state->cpus.size() - 1);
                            for (CoreCPU* cpu : state->cpus) {
                                if (cpu != this && !cpu->isBackingStoreLocked) {
                                    eligibleCPUs.push_back(cpu);
                                }
                            }

                            std::random_device rd;
                            std::mt19937 gen(rd());

                            if (eligibleCPUs.size() > 0) {
                                std::uniform_int_distribution<> distrib(0, eligibleCPUs.size() - 1);

                                std::vector<bool> usedArray(eligibleCPUs.size(), false);
                                int attempts = 0;
                                bool allocationSuccessful = false;

                                bool isCurrentProcessBackingStored = false;

                                while (attempts < eligibleCPUs.size() && !allocationSuccessful) {
                                    int index = distrib(gen);

                                    if (!usedArray[index]) {
                                        usedArray[index] = true;

                                        CoreCPU* randomCore = eligibleCPUs[index];
                                        std::lock(this->backstoreMutex, randomCore->backstoreMutex);
                                        std::lock_guard<std::mutex> lockAttacker(this->backstoreMutex, std::adopt_lock);
                                        std::lock_guard<std::mutex> lockDefender(randomCore->backstoreMutex, std::adopt_lock);
                                        isBackingStoreLocked = true;


                                        if (!this->current_process) {
                                            isCurrentProcessBackingStored = true;
                                            break;
                                        }

                                        if (randomCore->current_process) {
                                            Process* process = randomCore->current_process;
                                            process->storeToBackingStore(printer);
                                            state->pushProcessToBackingStore(process);
                                            if (state->getUsedAddress(process->id) != NOT_ALLOCATED) {
                                                state->allocator->deallocate(state->getUsedAddress(process->id));
                                                state->setUsedAddress(process->id, NOT_ALLOCATED);
                                            }

                                            state->ready_queue.return_to_process_queue(process);
                                            randomCore->current_process = nullptr;

                                            int newAddress = state->allocator->allocate(config, state, printer, this->current_process);
                                            state->setUsedAddress(this->current_process->id, newAddress);

                                            printer->debug(this->name + ": " + process->name + " To Backstore in Favor for " + current_process->name);

                                            state->cv.notify_one();

                                            if (newAddress != NOT_ALLOCATED) {
                                                // Allocation successful
                                                //state->cv.notify_one();
                                                allocationSuccessful = true;
                                                break; // Exit the inner while loop
                                            }

                                            else {
                                                break; //I'll check
                                            }
                                        }
                                    }
                                    attempts++;
                                }

                                if (isCurrentProcessBackingStored)
                                    continue;

                            }

                            else {
                                //printer->print(this->name + ": Ok I keep tracking " + current_process->name);
                                //goto ThirdAttempt;
                            }
                        }

                        //Label:
                        ///Third attempt...
                        {
                            std::lock_guard<std::mutex> lock(this->backstoreMutex);

                            if (current_process && state->getUsedAddress(current_process->id) == NOT_ALLOCATED) {
                                printer->debug("Process " + current_process->name + " is currently the selected one");
                                int processID = state->getRandomUsedAddressIndex(printer);

                                Process* process = nullptr;

                                if (processID != NOT_ALLOCATED) {
                                    process = state->ready_queue.getProcessFromIDAndPop(processID);
                                    if (process) {
                                        process->storeToBackingStore(printer);
                                        state->pushProcessToBackingStore(process);
                                        state->allocator->deallocate(state->getUsedAddress(processID));
                                        state->setUsedAddress(process->id, NOT_ALLOCATED); //Because we deallocated, not anymore on use. For now
                                        // printer->print(this->name + ": Memory is " + std::to_string(state->allocator->getTotalUsedMemory()) + ", but process requires " + std::to_string(process->memory));
                                        state->setUsedAddress(current_process->id, state->allocator->allocate(config, state, printer, current_process));
                                        // printer->print(this->name + ": Is used address = " + std::to_string(state->getUsedAddress(current_process->id)));
                                        printer->debug("Process " + process->name + " moved to backing store from ready queue");
                                        state->cv.notify_one();
                                    }
                                    else {
                                        printer->debug(this->name + ": A process with ID " + std::to_string(processID) + " is not present in Ready Queue");
                                    }
                                }

                                else {
                                    printer->debug(this->name + ": No process currently allocated.");
                                }

                                if (state->getUsedAddress(current_process->id) == NOT_ALLOCATED) {
                                    //printer->print(this->name + ": Failed 3rd attempt allocation for: " + current_process->name);
                                    state->ready_queue.return_to_process_queue(current_process);
                                    current_process = nullptr;
                                    this->idleTicks++;
                                    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(config->delaysPerExec * 1000)));
                                    continue;
                                }

                                else {
                                    // printer->print(this->name + ": Success 3rd attempt allocation for: " + current_process->name);
                                }
                            }

                            else {
                                if (!current_process) {
                                    printer->debug(this->name + ": Current Process is Null... ");
                                    this->idleTicks++;
                                    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(config->delaysPerExec * 1000)));
                                    continue;
                                }

                                else {
                                    //Success
                                }
                                //printer->print(this->name + ": Success 2nd attempt allocation for: " + current_process->name);
                            }
                        }
                    }


                    else {
                        printer->debug(this->name + ": Success 1st attempt allocation for: " + current_process->name);
                    }

                }

                else if (current_process) {
                    //printer->print(this->name + ": Already allocated = " + current_process->name);
                }

                else {
                    //printer->print(this->name + ": No process available");
                }
            }


            while (true) {
                {
                    std::lock_guard<std::mutex> backstoreLock(backstoreMutex);
                    std::lock_guard<std::mutex> lock(printMutex);

                    if (!current_process)
                        break;

                    std::string command = current_process->print_commands.front();
                    current_process->print_commands.pop();
                    current_process->completed_count++;
                    current_process->latest_time = format_time(std::chrono::system_clock::now());
                    this->activeTicks++;

                    if (current_process->print_commands.empty()) {

                        state->ready_queue.on_finish_process(current_process->id);
                        state->allocator->deallocate(state->getUsedAddress(current_process->id));
                        state->setUsedAddress(current_process->id, NOT_ALLOCATED);
                        break;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(config->delaysPerExec * 1000)));
            }
        }
    }
    else if (config->schedulerType == "rr") {

        while (true) {
            current_process = nullptr;
            Process* holder = nullptr;
            isBackingStoreLocked = false;
            {
                std::unique_lock<std::mutex> lock(mutex);

                state->cv.wait(lock, [state]() {
                    //printer->print(this->name + ": " + std::to_string(!state->ready_queue.process_queue.empty() || !state->backing_store.empty()));
                    return !state->ready_queue.is_process_queue_empty() || !state->isBackingStoreEmpty();
                    });

                if (state->ready_queue.is_process_queue_empty() && !state->isBackingStoreEmpty()) {
                    current_process = state->getProcessFromBackingStore(printer);
                }

                else {
                    current_process = state->ready_queue.on_process_running();
                    if (current_process) {
                        state->resizeUsedAddresses(current_process->id);
                    }
                }

                //If process is not null, and not yet allocated.
                if (current_process && (state->getUsedAddress(current_process->id) == NOT_ALLOCATED)) {
                    //printer->print(this->name + ": Allocating for " + current_process->name);
                    state->setUsedAddress(current_process->id, state->allocator->allocate(config, state, printer, current_process));

                    //First attempt
                    if (state->getUsedAddress(current_process->id) == NOT_ALLOCATED) {
                        //Second attempt
                        if (state->cpus.size() > 1) {
                            // Create a list of eligible CPUs (all except the current CPU)
                            std::vector<CoreCPU*> eligibleCPUs;
                            eligibleCPUs.reserve(state->cpus.size() - 1);
                            for (CoreCPU* cpu : state->cpus) {
                                if (cpu != this && !cpu->isBackingStoreLocked) {
                                    eligibleCPUs.push_back(cpu);
                                }
                            }

                            std::random_device rd;
                            std::mt19937 gen(rd());

                            if (eligibleCPUs.size() > 0) {
                                std::uniform_int_distribution<> distrib(0, eligibleCPUs.size() - 1);

                                std::vector<bool> usedArray(eligibleCPUs.size(), false);
                                int attempts = 0;
                                bool allocationSuccessful = false;

                                bool isCurrentProcessBackingStored = false;

                                while (attempts < eligibleCPUs.size() && !allocationSuccessful) {
                                    int index = distrib(gen);

                                    if (!usedArray[index]) {
                                        usedArray[index] = true;

                                        CoreCPU* randomCore = eligibleCPUs[index];
                                        std::lock(this->backstoreMutex, randomCore->backstoreMutex);
                                        std::lock_guard<std::mutex> lockAttacker(this->backstoreMutex, std::adopt_lock);
                                        std::lock_guard<std::mutex> lockDefender(randomCore->backstoreMutex, std::adopt_lock);
                                        isBackingStoreLocked = true;

                                        //The first thread sets the current_process to null on success of backing store
                                        //This is why it crashes, since its nullptr
                                        //They are trying to backing store each other...

                                        //isCurrentProcessBackingStored = this specific thread's process is backstored
                                        if (!this->current_process) {
                                            isCurrentProcessBackingStored = true;
                                            break;
                                        }

                                        if (randomCore->current_process) {
                                            Process* process = randomCore->current_process;
                                            process->storeToBackingStore(printer);
                                            state->pushProcessToBackingStore(process);
                                            if (state->getUsedAddress(process->id) != NOT_ALLOCATED) {
                                                state->allocator->deallocate(state->getUsedAddress(process->id));
                                                state->setUsedAddress(process->id, NOT_ALLOCATED);
                                            }
                                            //state->ready_queue.back_to_queue();
                                            state->ready_queue.return_to_process_queue(process);
                                            randomCore->current_process = nullptr;

                                            int newAddress = state->allocator->allocate(config, state, printer, this->current_process);
                                            state->setUsedAddress(this->current_process->id, newAddress);

                                            printer->debug(this->name + ": " + process->name + " To Backstore in Favor for " + current_process->name);

                                            state->cv.notify_one();

                                            if (newAddress != NOT_ALLOCATED) {
                                                // Allocation successful
                                                //state->cv.notify_one();
                                                allocationSuccessful = true;
                                                break; // Exit the inner while loop
                                            }

                                            else {
                                                goto ThirdAttempt;
                                            }
                                        }
                                    }
                                    attempts++;
                                }

                                if (isCurrentProcessBackingStored)
                                    continue;

                            }

                            else {
                                //printer->print(this->name + ": Ok I keep tracking " + current_process->name);
                                //goto ThirdAttempt;
                            }
                        }

                    ThirdAttempt:
                        ///Third attempt...
                        {
                            std::lock_guard<std::mutex> lock(this->backstoreMutex);

                            if (current_process && state->getUsedAddress(current_process->id) == NOT_ALLOCATED) {
                                printer->debug("Process " + current_process->name + " is currently the selected one");
                                int processID = state->getRandomUsedAddressIndex(printer);

                                Process* process = nullptr;

                                if (processID != NOT_ALLOCATED) {
                                    process = state->ready_queue.getProcessFromIDAndPop(processID);
                                    if (process) {
                                        process->storeToBackingStore(printer);
                                        state->pushProcessToBackingStore(process);
                                        state->allocator->deallocate(state->getUsedAddress(processID));
                                        state->setUsedAddress(process->id, NOT_ALLOCATED); //Because we deallocated, not anymore on use. For now
                                        // printer->print(this->name + ": Memory is " + std::to_string(state->allocator->getTotalUsedMemory()) + ", but process requires " + std::to_string(process->memory));
                                        state->setUsedAddress(current_process->id, state->allocator->allocate(config, state, printer, current_process));
                                        // printer->print(this->name + ": Is used address = " + std::to_string(state->getUsedAddress(current_process->id)));
                                        printer->debug("Process " + process->name + " moved to backing store from ready queue");
                                        state->cv.notify_one();
                                    }
                                    else {
                                        printer->debug(this->name + ": A process with ID " + std::to_string(processID) + " is not present in Ready Queue");
                                    }
                                }

                                else {
                                    printer->debug(this->name + ": No process currently allocated.");
                                }

                                if (state->getUsedAddress(current_process->id) == NOT_ALLOCATED) {
                                    //printer->print(this->name + ": Failed 3rd attempt allocation for: " + current_process->name);
                                    state->ready_queue.return_to_process_queue(current_process);
                                    current_process = nullptr;
                                    this->idleTicks++;
                                    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(config->delaysPerExec * 1000)));
                                    continue;
                                }

                                else {
                                    // printer->print(this->name + ": Success 3rd attempt allocation for: " + current_process->name);
                                }
                            }

                            else {
                                if (!current_process) {
                                    printer->debug(this->name + ": Current Process is Null... ");
                                    this->idleTicks++;
                                    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(config->delaysPerExec * 1000)));
                                    continue;
                                }

                                else {
                                    //Success
                                }
                                //printer->print(this->name + ": Success 2nd attempt allocation for: " + current_process->name);
                            }
                        }
                    }


                    else {
                        printer->debug(this->name + ": Success 1st attempt allocation for: " + current_process->name);
                    }

                }

                else if (current_process) {
                    //printer->print(this->name + ": Already allocated = " + current_process->name);
                }

                else {
                    //printer->print(this->name + ": No process available");
                }
            }

            if (current_process) {
                printer->debug(this->name + ": Process " + current_process->name + " running...");
                int quantum = config->quantumCycles - 1; //We are similar to do-while loop, we subtract.
                bool process_completed = false;

                while (true) {
                    bool should_continue = false;
                    {
                        std::lock_guard<std::mutex> backstoreLock(backstoreMutex);

                        if (current_process) {
                            std::lock_guard<std::mutex> printLock(printMutex);
                            if (!current_process->print_commands.empty()) { //
                                std::string command = current_process->print_commands.front();
                                current_process->print_commands.pop();
                                current_process->completed_count++;

                                this->activeTicks++;

                                if (current_process->print_commands.empty()) {
                                    printer->debug("Process " + current_process->name + " finished");
                                    state->allocator->deallocate(state->getUsedAddress(current_process->id));
                                    state->setUsedAddress(current_process->id, NOT_ALLOCATED);
                                    state->ready_queue.on_finish_process(current_process->id);
                                    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(config->delaysPerExec * 1000)));
                                    break;
                                }

                                else {
                                    should_continue = true;
                                }
                            }

                            /*else {
                                printer->print("Process " + current_process->name + " finished");
                                state->allocator->deallocate(state->getUsedAddress(current_process->id));
                                state->setUsedAddress(current_process->id, -1);
                                state->ready_queue.on_finish_process(current_process->id);
                                std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(config->delaysPerExec * 1000)));
                                break;
                            }*/

                        }

                        else {
                            break;
                        }
                    }

                    if (quantum > 0 && should_continue) {
                        quantum--;
                        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(config->delaysPerExec * 1000)));
                    }

                    else {
                        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(config->delaysPerExec * 1000)));
                        break;
                    }
                }

                {
                    std::lock_guard<std::mutex> backstoreLock(backstoreMutex);
                    if (current_process) {
                        std::lock_guard<std::mutex> lock(printMutex);
                        if (!current_process->print_commands.empty()) {
                            printer->debug(this->name + ": Process " + current_process->name + " is returned to process queue");
                            state->ready_queue.return_to_process_queue(current_process);
                        }
                        else {
                        }
                    }
                }
            }

            else {
                this->idleTicks++;
                std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(config->delaysPerExec * 1000)));
            }
        }
    }

    else if (config->schedulerType == "sjf") {
        current_process = nullptr;
        while (true) {
            {
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    isBackingStoreLocked = false;

                    state->cv.wait(lock, [state]() {
                        //printer->print(this->name + ": " + std::to_string(!state->ready_queue.process_queue.empty() || !state->backing_store.empty()));
                        return !state->ready_queue.is_process_queue_empty() || !state->isBackingStoreEmpty();
                        });

                    if (!current_process && state->ready_queue.is_process_queue_empty() && !state->isBackingStoreEmpty()) {
                        current_process = state->getProcessFromBackingStore(printer);
                    }

                    else {
                        current_process = state->ready_queue.shortest_process_first(config->preemptive, current_process);
                        if (current_process) {
                            state->resizeUsedAddresses(current_process->id);
                        }
                    }

                    //If process is not null, and not yet allocated.
                    if (current_process && (state->getUsedAddress(current_process->id) == NOT_ALLOCATED)) {
                        //printer->print(this->name + ": Allocating for " + current_process->name);
                        state->setUsedAddress(current_process->id, state->allocator->allocate(config, state, printer, current_process));

                        //First attempt
                        if (state->getUsedAddress(current_process->id) == NOT_ALLOCATED) {
                            //Second attempt
                            if (state->cpus.size() > 1) {
                                // Create a list of eligible CPUs (all except the current CPU)
                                std::vector<CoreCPU*> eligibleCPUs;
                                eligibleCPUs.reserve(state->cpus.size() - 1);
                                for (CoreCPU* cpu : state->cpus) {
                                    if (cpu != this && !cpu->isBackingStoreLocked) {
                                        eligibleCPUs.push_back(cpu);
                                    }
                                }

                                std::random_device rd;
                                std::mt19937 gen(rd());

                                if (eligibleCPUs.size() > 0) {
                                    std::uniform_int_distribution<> distrib(0, eligibleCPUs.size() - 1);

                                    std::vector<bool> usedArray(eligibleCPUs.size(), false);
                                    int attempts = 0;
                                    bool allocationSuccessful = false;

                                    bool isCurrentProcessBackingStored = false;

                                    while (attempts < eligibleCPUs.size() && !allocationSuccessful) {
                                        int index = distrib(gen);

                                        if (!usedArray[index]) {
                                            usedArray[index] = true;

                                            CoreCPU* randomCore = eligibleCPUs[index];
                                            std::lock(this->backstoreMutex, randomCore->backstoreMutex);
                                            std::lock_guard<std::mutex> lockAttacker(this->backstoreMutex, std::adopt_lock);
                                            std::lock_guard<std::mutex> lockDefender(randomCore->backstoreMutex, std::adopt_lock);
                                            isBackingStoreLocked = true;


                                            if (!this->current_process) {
                                                isCurrentProcessBackingStored = true;
                                                break;
                                            }

                                            if (randomCore->current_process) {
                                                Process* process = randomCore->current_process;
                                                process->storeToBackingStore(printer);
                                                state->pushProcessToBackingStore(process);
                                                if (state->getUsedAddress(process->id) != NOT_ALLOCATED) {
                                                    state->allocator->deallocate(state->getUsedAddress(process->id));
                                                    state->setUsedAddress(process->id, NOT_ALLOCATED);
                                                }

                                                state->ready_queue.return_to_process_queue(process);
                                                randomCore->current_process = nullptr;

                                                int newAddress = state->allocator->allocate(config, state, printer, this->current_process);
                                                state->setUsedAddress(this->current_process->id, newAddress);

                                                printer->debug(this->name + ": " + process->name + " To Backstore in Favor for " + current_process->name);

                                                state->cv.notify_one();

                                                if (newAddress != NOT_ALLOCATED) {
                                                    // Allocation successful
                                                    //state->cv.notify_one();
                                                    allocationSuccessful = true;
                                                    break; // Exit the inner while loop
                                                }

                                                else {
                                                    break; //I'll check
                                                }
                                            }
                                        }
                                        attempts++;
                                    }

                                    if (isCurrentProcessBackingStored)
                                        continue;

                                }

                                else {
                                    //printer->print(this->name + ": Ok I keep tracking " + current_process->name);
                                    //goto ThirdAttempt;
                                }
                            }

                            //Label:
                            ///Third attempt...
                            {
                                std::lock_guard<std::mutex> lock(this->backstoreMutex);

                                if (current_process && state->getUsedAddress(current_process->id) == NOT_ALLOCATED) {
                                    printer->debug("Process " + current_process->name + " is currently the selected one");
                                    int processID = state->getRandomUsedAddressIndex(printer);

                                    Process* process = nullptr;

                                    if (processID != NOT_ALLOCATED) {
                                        process = state->ready_queue.getProcessFromIDAndPop(processID);
                                        if (process) {
                                            process->storeToBackingStore(printer);
                                            state->pushProcessToBackingStore(process);
                                            state->allocator->deallocate(state->getUsedAddress(processID));
                                            state->setUsedAddress(process->id, NOT_ALLOCATED); //Because we deallocated, not anymore on use. For now
                                            // printer->print(this->name + ": Memory is " + std::to_string(state->allocator->getTotalUsedMemory()) + ", but process requires " + std::to_string(process->memory));
                                            state->setUsedAddress(current_process->id, state->allocator->allocate(config, state, printer, current_process));
                                            // printer->print(this->name + ": Is used address = " + std::to_string(state->getUsedAddress(current_process->id)));
                                            printer->debug("Process " + process->name + " moved to backing store from ready queue");
                                            state->cv.notify_one();
                                        }
                                        else {
                                            printer->debug(this->name + ": A process with ID " + std::to_string(processID) + " is not present in Ready Queue");
                                        }
                                    }

                                    else {
                                        printer->debug(this->name + ": No process currently allocated.");
                                    }

                                    if (state->getUsedAddress(current_process->id) == NOT_ALLOCATED) {
                                        //printer->print(this->name + ": Failed 3rd attempt allocation for: " + current_process->name);
                                        state->ready_queue.return_to_process_queue(current_process);
                                        current_process = nullptr;
                                        this->idleTicks++;
                                        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(config->delaysPerExec * 1000)));
                                        continue;
                                    }

                                    else {
                                        // printer->print(this->name + ": Success 3rd attempt allocation for: " + current_process->name);
                                    }
                                }

                                else {
                                    if (!current_process) {
                                        printer->debug(this->name + ": Current Process is Null... ");
                                        this->idleTicks++;
                                        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(config->delaysPerExec * 1000)));
                                        continue;
                                    }

                                    else {
                                        //Success
                                    }
                                    //printer->print(this->name + ": Success 2nd attempt allocation for: " + current_process->name);
                                }
                            }
                        }


                        else {
                            printer->debug(this->name + ": Success 1st attempt allocation for: " + current_process->name);
                        }

                    }

                    else if (current_process) {
                        //printer->print(this->name + ": Already allocated = " + current_process->name);
                    }

                    else {
                        //printer->print(this->name + ": No process available");
                    }
                }


                if (current_process) {
                    this->activeTicks++;
                    std::lock_guard<std::mutex> backstoreLock(backstoreMutex);
                    std::lock_guard<std::mutex> printLock(printMutex);
                    if (!current_process->print_commands.empty()) {
                        current_process->print_commands.pop();
                        current_process->completed_count++;
                        current_process->latest_time = format_time(std::chrono::system_clock::now());

                        if (current_process->print_commands.empty()) {
                            state->ready_queue.on_finish_process(current_process->id);
                            current_process = nullptr;
                        }
                    }
                }

                else {
                    this->idleTicks++;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(config->delaysPerExec * 1000)));
        }
    }
}

CoreCPU::CoreCPU(std::string name, BufferPrint* p, SchedulerState* s, SchedulerConfig* c) : name(name) {

    std::thread processing_unit(&CoreCPU::process_realtime, this, p, s, c);
    processing_unit.detach();
}

// SchedulerState implementations
void SchedulerState::moveRandomProcessToBackingStore(CoreCPU* except, SchedulerConfig* config, SchedulerState* state, BufferPrint* printer) {
    /*if (cpus.size() <= 1) return;  // No other CPUs to choose from

    // Create a list of eligible CPUs (all except the 'except' CPU)
    std::vector<CoreCPU*> eligibleCPUs;
    eligibleCPUs.reserve(cpus.size() - 1);
    for (CoreCPU* cpu : cpus) {
        if (cpu != except) {
            eligibleCPUs.push_back(cpu);
        }
    }

    int size = eligibleCPUs.size();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, size - 1);

    std::vector<int> usedArray;

    for (int i = 0; i < size; i++) {
        usedArray.push_back(-1);
    }
    // Randomly select from eligible CPU

    int index = distrib(gen);
    int attempts = 0;
    if (usedArray[index] == 1) {
        // Find the next available index
        do {
            index = (index + 1) % usedArray.size();
            if (attempts >= usedArray.size()) {
                //printer->print(randomCore->name + ": All cores have no backing store...");
                return;
            }
        } while (usedArray[index] == 1);
    }

    usedArray[index] = 1;

    CoreCPU* randomCore = eligibleCPUs[index];
    {
        std::lock(except->backstoreMutex, randomCore->backstoreMutex);
        std::lock_guard<std::mutex> lockAttacker(except->backstoreMutex, std::adopt_lock);
        std::lock_guard<std::mutex> lockDefender(randomCore->backstoreMutex, std::adopt_lock);
        if (randomCore->current_process) {
            Process* process = randomCore->current_process;
            process->storeToBackingStore(printer);
            backing_store.push_back(process);
            state->allocator->deallocate((*state->usedAddresses)[process->id]);
            randomCore->current_process = nullptr;
            state->setUsedAddress(except->current_process->id, state->allocator->allocate(config, state, printer, except->current_process));
            //printer->print(randomCore->name + ": Process " + process->name + " moved to backing store");
            this->cv.notify_one();
        }
        else {

        }
    }
    */
}

void SchedulerState::resizeUsedAddresses(int size) {
    std::lock_guard<std::mutex> resizeLock(this->resizeMutex);
    if (this->usedAddresses->size() <= size)
        usedAddresses->resize(size * 2 + 1, NOT_ALLOCATED);
}

int SchedulerState::getUsedAddress(int index) {
    std::lock_guard<std::mutex> resizeLock(this->resizeMutex);
    return (*this->usedAddresses)[index];
}

void SchedulerState::setUsedAddress(int index, int value) {
    std::lock_guard<std::mutex> resizeLock(this->resizeMutex);
    (*this->usedAddresses)[index] = value;
}

void SchedulerState::pushProcessToBackingStore(Process* process) {
    std::lock_guard<std::mutex> resizeLock(this->pushMutex);
    this->backing_store->push_back(process);
}

bool SchedulerState::isBackingStoreEmpty() {
    std::lock_guard<std::mutex> resizeLock(this->pushMutex);
    return this->backing_store->empty();
}
int SchedulerState::getBackingStoreSize() {
    std::lock_guard<std::mutex> resizeLock(this->pushMutex);
    return this->backing_store->size();
}

//int SchedulerState::peekLastBackingStoreProcessID() {
//    std::lock_guard<std::mutex> resizeLock(this->pushMutex);
//    if (this->backing_store->empty())
//        return NOT_ALLOCATED;
//    return this->backing_store->back()->id;
//}



int SchedulerState::getRandomUsedAddressIndex(BufferPrint* printer) {
    std::lock_guard<std::mutex> resizeLock(this->resizeMutex);
    auto& addressArray = (*this->usedAddresses);
    // Filter out -1 entries and store valid addresses
    std::vector<int> validIndices;
    for (int i = 0; i < addressArray.size(); i++) {
        if (addressArray[i] != NOT_ALLOCATED) {
            validIndices.push_back(i);
            //printer->print("Valid Index: " + std::to_string(i) + " has " + std::to_string(addressArray[i]) + " value");
        }
    }
    // Check if there are any valid addresses
    if (validIndices.empty()) {
        //throw std::runtime_error("No valid addresses found");
        return NOT_ALLOCATED;
    }
    // Generate a random index
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, validIndices.size() - 1);
    int randomIndex = dis(gen);
    // Return the randomly selected valid index

   // printer->print("Valid Index returned: " + std::to_string(validIndices[randomIndex]));
    return validIndices[randomIndex];
}

//Process* SchedulerState::getProcessFromBackingStore(BufferPrint* printer) {
//    std::lock_guard<std::mutex> lock(this->pushMutex);
//    if (!backing_store->empty()) {
//        Process* process = backing_store->back();
//        backing_store->pop_back();
//        process->returnFromBackingStore(printer);
//        //printer->print("Process " + process->name + " returned from backing store.");
//        return process;
//    }
//    return nullptr;
//}
Process* SchedulerState::getProcessFromBackingStore(BufferPrint* printer) {
    std::lock_guard<std::mutex> lock(this->pushMutex);
    if (!backing_store->empty()) {
        // Generate a random index
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, backing_store->size() - 1);
        int randomIndex = dis(gen);

        // Get the process at the random index
        auto it = backing_store->begin() + randomIndex;
        Process* process = *it;

        // Remove the process from the backing store
        backing_store->erase(it);

        process->returnFromBackingStore(printer);
        //printer->print("Process " + process->name + " returned from backing store.");
        return process;
    }
    return nullptr;
}

Process* SchedulerState::getSpecificProcessFromBackingStore(int id, BufferPrint* printer) {
    std::lock_guard<std::mutex> lock(this->pushMutex);
    if (!backing_store->empty()) {
        auto it = std::find_if(backing_store->begin(), backing_store->end(),
            [id](const Process* p) { return p->id == id; });

        if (it != backing_store->end()) {
            Process* process = *it;
            backing_store->erase(it);
            process->returnFromBackingStore(printer);
            printer->print("Process " + process->name + " returned from backing store.");
            return process;
        }
    }
    return nullptr;
}


//Marque Start ============================================================================================================
const int FPS = 40;

class Utility {
public:
    static std::mutex consoleMutex;

    static void setCursorPosition(SHORT x, SHORT y) {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        COORD coord = { x, y };
        SetConsoleCursorPosition(hConsole, coord);
    }

    static void writeToConsole(SHORT x, SHORT y, const std::string& text) {
        std::lock_guard<std::mutex> lock(consoleMutex);
        setCursorPosition(x, y);
        std::cout << text;
    }

    static void clearLine(int y) {
        std::lock_guard<std::mutex> lock(consoleMutex);
        setCursorPosition(0, y);
        std::cout << "\033[K";
    }
};

std::mutex Utility::consoleMutex;
bool isMarqueeExited = false;

class PersistentText {
public:
    std::string inputted;
    std::vector<std::string> log;
};

class BouncingText {
public:
    BouncingText(SHORT x, SHORT y, std::string print) : x(x), y(y), print(print), mX(1), mY(1) {}

    SHORT x, y;
    SHORT mX, mY;
    std::string print;

    void move() {
        x += mX;
        y += mY;
        Utility::writeToConsole(x, y, print);
    }

    void adjust() {
        HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);

        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(console, &csbi);

        int consoleWidth = 98;
        int consoleHeight = 8 + 3 + 6;

        Utility::clearLine(y + 2);
        Utility::clearLine(y + 1);
        Utility::clearLine(y);
        Utility::clearLine(y - 1);
        Utility::clearLine(y - 2);

        if (x >= consoleWidth) {
            mX = -1;
        }
        else if (x <= 0) {
            mX = 1;
        }
        if (y >= consoleHeight) {
            mY = -1;
        }
        else if (y <= 1 + 5) {
            mY = 1;
        }
    }
};

void displayBouncingTextThreaded(BouncingText& text) {
    const auto targetFrameTime = std::chrono::microseconds(1000000 / FPS);
    while (!isMarqueeExited) {
        auto startTime = std::chrono::high_resolution_clock::now();

        text.adjust();
        text.move();

        auto endTime = std::chrono::high_resolution_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        std::this_thread::sleep_for(targetFrameTime - elapsedTime);
    }
}

void headerThreaded() {
    const auto targetFrameTime = std::chrono::microseconds(1000000 / FPS);

    while (!isMarqueeExited) {
        auto startTime = std::chrono::high_resolution_clock::now();

        Utility::writeToConsole(0, 0, "**************************************");
        Utility::writeToConsole(0, 1, "   *  Displaying a marquee console! *  ");
        Utility::writeToConsole(0, 2, "**************************************");

        auto endTime = std::chrono::high_resolution_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        std::this_thread::sleep_for(targetFrameTime - elapsedTime);
    }
}

void keyPollThreaded(PersistentText& text) {
    const auto targetFrameTime = std::chrono::microseconds(1000000 / FPS);

    std::thread displayKeyboardCommand([&text]() {
        const auto targetFrameTime = std::chrono::microseconds(1000000 / FPS);
        while (!isMarqueeExited) {
            auto startTime = std::chrono::high_resolution_clock::now();
            std::string input = "Enter a command for MARQUEE_CONSOLE: " + text.inputted;

            int baseY = 3 + 6 + 8 + 1 + 6;
            Utility::clearLine(baseY);
            Utility::writeToConsole(0, baseY, input);

            for (size_t size = 0; size < text.log.size(); size++) {
                Utility::clearLine(size + baseY + 1);
                std::string logged_text = "Command processed in MARQUEE_CONSOLE: " + text.log[size];
                Utility::writeToConsole(0, size + baseY + 1, logged_text);
            }

            Utility::setCursorPosition(input.length(), baseY);
            auto endTime = std::chrono::high_resolution_clock::now();
            auto elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
            std::this_thread::sleep_for(targetFrameTime - elapsedTime);
        }
        });

    while (!isMarqueeExited) {
        if (_kbhit()) {
            char key = _getch();

            if (key == '\r') {  // Enter key
                if (text.inputted == "exit") {
                    isMarqueeExited = true;
                    break;
                }
                else {
                    text.log.push_back(text.inputted);
                    text.inputted = std::string();  // Clear the input string for the next line
                }
            }
            else if (key == '\b') {  // Backspace key
                if (!text.inputted.empty()) {
                    text.inputted.pop_back();  // Remove the last character from the string
                }
            }
            else {
                text.inputted += key;  // Concatenate the character to the input string
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    displayKeyboardCommand.join();
}

void marquee() {
    isMarqueeExited = false;

    BouncingText bouncingText(0, 3, "This is a marquee text");
    PersistentText persistentText;

    std::thread headerThread(headerThreaded);
    std::thread bouncingTextThread([&bouncingText]() { displayBouncingTextThreaded(bouncingText); });
    std::thread persistentTextThread([&persistentText]() { keyPollThreaded(persistentText); });

    headerThread.join();
    bouncingTextThread.join();
    persistentTextThread.join();
}