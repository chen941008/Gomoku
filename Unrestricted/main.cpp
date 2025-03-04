#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

#include "Game.hpp"
#include "MCTS.hpp"
#include "Node.hpp"

using namespace std;
int main() {
    /*
    cout << "Welcome to Gomoku" << endl;
    int gameTimes;
    cout << "Input how many game you want to play." << endl;
    cin >> gameTimes;
    for (int i = 0; i < gameTimes; i++) {
        Game::startGame();
    }
    */
    initializePrecomputed();
    int gameTimes = 5;
    long long totalTime = 0;
    ofstream outputFile("bitboard_main_threadV1.csv");
    if (!outputFile.is_open()) {
        cerr << "Error: Unable to open output file!" << endl;
        return 1;
    }
    // 寫入 CSV 標題行
    outputFile << "Core,SimulationTimes,AverageTime (ms)" << endl;
    // 模擬不同次數的情況
    for (int i = 6; i <= 6; i++) {
        for (int simulationTimes = 1000; simulationTimes <= 5000; simulationTimes += 1000) {
            totalTime = 0;
            for (int j = 0; j < gameTimes; j++) {
                Node* root = new Node();  // 創建根節點
                // Game::generateFullTree(root);        // 生成完整遊戲樹
                MCTS ai(simulationTimes, i);       // 創建 MCTS AI
                totalTime += ai.run(root, 10000);  // 執行 MCTS
                deleteTree(root);                  // 刪除樹
            }
            // 計算平均時間
            double average_time = totalTime / static_cast<double>(gameTimes);
            // 輸出到 CSV 檔案
            outputFile << i << "," << simulationTimes << "," << average_time << endl;
            // 同時輸出到控制台（可選）
            cout << "Core = " << i << ", SimulationTimes = " << simulationTimes << ", AverageTime = " << average_time
                 << " ms" << endl;
        }
    }
    /*
    for (int simulationTimes = 1000; simulationTimes <= 10000; simulationTimes += 1000) {
        totalTime = 0;
        for (int i = 0; i < gameTimes; i++) {
            Node* root = new Node();  // 創建根節點
            // Game::generateFullTree(root);        // 生成完整遊戲樹
            MCTS ai(simulationTimes, 5);       // 創建 MCTS AI
            totalTime += ai.run(root, 10000);  // 執行 MCTS
            deleteTree(root);                  // 刪除樹
        }
        // 計算平均時間
        double average_time = totalTime / static_cast<double>(gameTimes);
        // 輸出到 CSV 檔案
        outputFile << simulationTimes << "," << average_time << endl;
        // 同時輸出到控制台（可選）
        cout << "SimulationTimes = " << simulationTimes << ", AverageTime = " << average_time << " ms" << endl;
    }
    // 關閉檔案
    outputFile.close();
    */
    return 0;
}