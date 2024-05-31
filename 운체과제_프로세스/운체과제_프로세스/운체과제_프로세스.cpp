#include <iostream>
#include <list>
#include <thread>
#include <mutex>
#include <chrono>

using namespace std;

// 프로세스 구조체
struct Process {
    int id; // 프로세스 ID
    bool isForeground; // 프로세스가 foreground인지 여부
    bool isPromoted; // 프로세스가 프로모션된 상태인지 여부
    int remainingTime; // 프로세스의 남은 시간 (초)
    // 추가 정보 포함 가능
};

// 스택 노드 구조체
struct StackNode {
    list<Process> processes; // 프로세스 리스트
    StackNode* next; // 다음 스택 노드를 가리키는 포인터
    StackNode() : next(nullptr) {}
};

class ProcessStack {
private:
    StackNode* top; // 스택의 맨 위를 가리키는 포인터
    StackNode* promotePointer; // 프로모션을 위한 포인터
    int threshold; // 리스트 분할 기준 임계값
    mutex mtx; // 상태 출력 동기화를 위한 뮤텍스

    // 스택 내부에서의 프로세스 리스트 분할 및 병합
    void splitAndMerge(StackNode* node) {
        // 리스트 길이가 임계값을 넘어서면 분할
        if (node == nullptr || node->processes.size() <= threshold) return;

        auto it = node->processes.begin();
        advance(it, node->processes.size() / 2);

        list<Process> newList(it, node->processes.end());
        node->processes.erase(it, node->processes.end());

        // 다음 스택 노드가 없으면 새로 생성
        if (node->next == nullptr) {
            node->next = new StackNode();
        }
        node->next->processes.splice(node->next->processes.end(), newList);

        // 재귀적으로 분할 및 병합 진행
        splitAndMerge(node->next);
    }

    // 프로모션을 위한 포인터 이동
    void movePromotePointer() {
        if (promotePointer == nullptr || promotePointer->next == nullptr) {
            promotePointer = top;
        }
        else {
            promotePointer = promotePointer->next;
        }
    }

    // 프로세스 상태 출력
    void printProcessStatus(Process process) {
        cout << process.id << (process.isForeground ? "F" : "B");
        if (process.isPromoted) {
            cout << "*";
        }
        cout << " (Remaining Time: " << process.remainingTime << "s)";
    }

public:
    ProcessStack() : top(new StackNode()), promotePointer(top), threshold(5) {}

    ~ProcessStack() {
        while (top != nullptr) {
            StackNode* temp = top;
            top = top->next;
            delete temp;
        }
    }

    // 프로세스 enqueue
    void enqueue(Process process, bool isBackground) {
        lock_guard<mutex> lock(mtx); // 상태 출력 동기화
        if (isBackground) {
            top->processes.push_back(process);
        }
        else {
            top->processes.push_front(process);
        }
        splitAndMerge(top);
    }

    // 프로세스 dequeue
    bool dequeue() {
        lock_guard<mutex> lock(mtx); // 상태 출력 동기화
        if (top == nullptr || top->processes.empty()) return false;
        top->processes.pop_front();
        if (top->processes.empty()) {
            StackNode* temp = top;
            top = top->next;
            delete temp;
            if (promotePointer == temp) {
                promotePointer = top;
            }
        }
        return true;
    }

    // 프로모션
    void promote() {
        lock_guard<mutex> lock(mtx); // 상태 출력 동기화
        if (promotePointer == nullptr || promotePointer->next == nullptr) return;

        Process process = promotePointer->next->processes.front();
        promotePointer->next->processes.pop_front();
        promotePointer->processes.push_back(process);

        if (promotePointer->next->processes.empty()) {
            StackNode* temp = promotePointer->next;
            promotePointer->next = promotePointer->next->next;
            delete temp;
        }

        movePromotePointer();
    }

    // 스택 상태 출력
    void printStackStatus() {
        cout << "Running: ";
        StackNode* current = top;
        while (current != nullptr) {
            for (Process p : current->processes) {
                cout << "[" << p.id << (p.isForeground ? "F" : "B") << "]";
                if (current->next != nullptr) {
                    cout << " (bottom)";
                }
                cout << endl;
            }
            current = current->next;
        }
        cout << "------------------------------" << endl;
    }
};

// 프로세스 실행 함수
void processExecution(ProcessStack& stack, int X, int Y) {
    int foregroundId = 0;
    int backgroundId = 0;
    while (true) {
        // X초마다 DQ와 WQ 출력
        this_thread::sleep_for(chrono::seconds(X));
        cout << "------------------------------" << endl;
        cout << "DQ: ";
        // DQ 상태 출력
        stack.printStackStatus();
        cout << "WQ: " << endl;
        // WQ 상태 출력
        cout << "------------------------------" << endl;

        // Y초마다 shell 프로세스 실행
        this_thread::sleep_for(chrono::seconds(Y));
        // shell 프로세스 생성
        Process shellProcess;
        shellProcess.id = foregroundId++;
        shellProcess.isForeground = true;
        shellProcess.isPromoted = false;
        shellProcess.remainingTime = 0; // 즉시 종료되도록 설정
        // 셸 프로세스 enqueue
        stack.enqueue(shellProcess, true);
        // 셸 프로세스 dequeue
        stack.dequeue();
    }
}

int main() {
    ProcessStack stack;

    // 스케줄링 함수 실행
    thread schedulingThread(processExecution, ref(stack), 5, 10);

    // 스레드 종료 대기
    schedulingThread.join();

    return 0;
}

