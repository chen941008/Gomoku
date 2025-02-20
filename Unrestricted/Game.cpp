#include "Game.hpp"

#include <stdint.h>

#include <iostream>

#include "MCTS.hpp"
#include "Node.hpp"

// Function declaration for expansion

using namespace std;
void Game::startGame() {
    Node* root = new Node();
    // generateFullTree(root);
    Node* currentNode = root;  // CurrentNode為當前棋盤最後一個子的節點，會去選擇他的子節點來下棋
    int playerOrder, currentOrder = 0, aiMode, iterationTimes, simulationTimes;
    cout << "Input stimulation times." << endl;
    cin >> simulationTimes;
    MCTS ai(simulationTimes);
    ai.expansion(currentNode);
    cout << "Choose AI simulation mode: 1 = fixed simulation times, 2 = "
            "variable simulation times"
         << endl;
    while (true) {
        cin >> aiMode;
        if (aiMode == aiMode::FIXED_SIMULATION_TIMES) {
            while (true) {
                cout << "Input how many iteration you want to run (must be "
                        "greater than 225)."
                     << endl;
                cin >> iterationTimes;
                if (iterationTimes > MAX_CHILDREN) {
                    break;
                }
                cout << "Please input a number greater than 225." << endl;
            }
            break;
        }
        if (aiMode == aiMode::VARIABLE_SIMULATION_TIMES) {
            break;
        }
        cout << "Please input 1 or 2" << endl;
    }
    // 用 bitboard 表示棋盤，初始皆為 0
    int board[BOARD_SIZE][BOARD_SIZE] = {0};
    cout << "Choose first or second player, input 1 or 2" << endl;
    while (true) {  // 選擇先手或後手，防白痴crash程式
        cin >> playerOrder;
        if (playerOrder == 1 || playerOrder == 2) {
            break;
        }
        cout << "Please input 1 or 2" << endl;
    }
    playerOrder--;
    while (true) {
        if (currentOrder == MAX_CHILDREN) {
            cout << "Draw" << endl;
            printBoard(board);
            break;
        }
        printBoard(board);
        if (currentOrder % 2 == playerOrder) {  // Player turn
            cout << "Your turn" << endl;
            int X, Y;
            cout << "input X Y 0~14" << endl;
            while (true) {
                cin >> X >> Y;
                if (X < 0 || X > (BOARD_SIZE - 1) || Y < 0 || Y > (BOARD_SIZE - 1)) {
                    cout << "Please input 0~14" << endl;
                    continue;
                }
                // 檢查該位置是否已被佔用
                if (board[X][Y] != 0) {
                    cout << "This position is already taken" << endl;
                    continue;
                }
                break;
            }
            if (currentOrder % 2 == 0) {
                board[X][Y] = 1;
            } else {
                board[X][Y] = 2;
            }
            if (currentOrder >= CHECKWIN_THRESHOLD && checkWin(board, {X, Y})) {
                cout << "You win" << endl;
                printBoard(board);
                break;
            }
            for (int i = 0; i < MAX_CHILDREN && currentNode->children[i] != nullptr; i++) {
                if (currentNode->children[i]->lastMove.x == X && currentNode->children[i]->lastMove.y == Y) {
                    currentNode = currentNode->children[i];
                    break;
                }
            }
        } else {  // AI turn
            cout << "AI turn" << endl;
            if (aiMode == aiMode::VARIABLE_SIMULATION_TIMES) {
                cout << "Input how many iteration you want to run (must be "
                        "greater than 225)."
                     << endl;
                do {
                    cin >> iterationTimes;
                    if (iterationTimes <= MAX_CHILDREN) {
                        cout << "Please input a number greater than 225." << endl;
                    }
                } while (iterationTimes <= MAX_CHILDREN);
            }
            ai.run(currentNode, iterationTimes);
            Node* bestChild = nullptr;
            int mostVisit = 0;
            for (int i = 0; i < MAX_CHILDREN && currentNode->children[i] != nullptr; ++i) {
                Node* child = currentNode->children[i];
                if (child->visits > mostVisit) {
                    mostVisit = child->visits;
                    bestChild = child;
                }
            }
            Position lastMove = bestChild->lastMove;
            if (currentOrder % 2 == 0) {
                board[lastMove.x][lastMove.y] = 1;
            } else {
                board[lastMove.x][lastMove.y] = 2;
            }
            cout << "AI choose " << lastMove.x << " " << lastMove.y << endl;
            currentNode = bestChild;
            if (currentOrder >= CHECKWIN_THRESHOLD && checkWin(board, lastMove)) {
                cout << "AI win" << endl;
                printBoard(board);
                break;
            }
        }
        cout << "別急，我刪個分支" << endl;
        currentOrder++;
        Node* parent = currentNode->parent;
        for (int i = 0; i < MAX_CHILDREN && parent->children[i] != nullptr; i++) {
            if (parent->children[i] == currentNode) {
                continue;
            }
            deleteTree(parent->children[i]);
        }
    }
    delete root;
}

void Game::printBoard(int board[BOARD_SIZE][BOARD_SIZE]) {
    cout << endl;

    // 印出上方的欄位標題（0 ~ BOARD_SIZE-1）
    cout << "    ";  // 左上角空白區域
    for (int j = 0; j < BOARD_SIZE; j++) {
        // 為了對齊，假設 j 為單一數字時多留兩個空格，兩位數則一個空格
        if (j < 10)
            cout << j << "   ";
        else
            cout << j << "  ";
    }
    cout << endl;

    // 印出上方的分隔線
    cout << "   ";
    for (int j = 0; j < BOARD_SIZE; j++) {
        cout << "----";
    }
    cout << endl;

    // 印出每一列
    for (int i = 0; i < BOARD_SIZE; i++) {
        // 印出行號（左邊標題）
        if (i < 10)
            cout << i << "  |";
        else
            cout << i << " |";

        // 印出該行的每個棋子
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == 1) {
                cout << " X ";
            } else if (board[i][j] == 2) {
                cout << " O ";
            } else {
                cout << "   ";
            }
            if (j < BOARD_SIZE - 1) {
                cout << "|";
            }
        }
        cout << endl;

        // 每行後面加上分隔線（除了最後一行）
        if (i < BOARD_SIZE - 1) {
            cout << "   ";  // 與行號對齊的空白
            for (int j = 0; j < BOARD_SIZE; j++) {
                if (j == 0) {
                    cout << "-";
                }
                cout << "---";
                if (j < BOARD_SIZE - 1) {
                    cout << "+";
                }
            }
            cout << endl;
        }
    }
    // 印出下方的分隔線
    cout << "   ";
    for (int j = 0; j < BOARD_SIZE; j++) {
        cout << "----";
    }
    cout << endl;
}

/*
// 遞迴生成完整遊戲樹的函式
void Game::generateFullTree(Node* node) {
    // 取得當前棋盤已使用的位置
    uint16_t usedPositions = node->boardX | node->boardO;

    // 如果棋盤已滿，或者當前局面已經分出勝負，就不再展開
    if (usedPositions == 0b111111111) {
        node->state = BoardState::DRAW;
        return;
    } else if (checkWin(node->boardX, node->boardO, node->isXTurn)) {
        node->state = BoardState::WIN;
        return;
    }

    int childIndex = 0;
    // 對於棋盤上的每個位置，若未被使用，則產生一個新節點
    for (int i = 0; i < MAX_CHILDREN; i++) {
        if (!(usedPositions & (1 << i))) {      // 若位置 i 未被使用
            Node* newNode = new Node(i, node);  // 依據父節點狀態及這步 move 建立新節點
            node->children[childIndex++] = newNode;
        }
    }

    // 遞迴產生每個子節點的遊戲樹
    for (int i = 0; i < childIndex; i++) {
        generateFullTree(node->children[i]);
    }
}
*/
bool Game::checkWin(int board[BOARD_SIZE][BOARD_SIZE], Position lastMove) {
    int player = board[lastMove.x][lastMove.y];  // 獲取玩家（0 = 空，1 = 黑，2 = 白）

    if (player == 0) {
        return false;  // 如果該位置是空的，沒有勝利
    }

    // 定義方向，這些方向代表了水平、垂直、對角線方向
    const int directions[4][2] = {
        {1, 0},  // 水平方向（左右）
        {0, 1},  // 垂直方向（上下）
        {1, 1},  // 右下對角線
        {1, -1}  // 左下對角線
    };

    // 檢查四個方向
    for (int i = 0; i < 4; ++i) {
        int count = 1;  // 包括當前的步驟

        // 檢查正向（向某一方向延伸）
        for (int step = 1; step < 5; ++step) {
            int newX = lastMove.x + directions[i][0] * step;
            int newY = lastMove.y + directions[i][1] * step;
            if (newX >= 0 && newX < BOARD_SIZE && newY >= 0 && newY < BOARD_SIZE && board[newX][newY] == player) {
                count++;
            } else {
                break;
            }
        }

        // 檢查反向（向另一個方向延伸）
        for (int step = 1; step < 5; ++step) {
            int newX = lastMove.x - directions[i][0] * step;
            int newY = lastMove.y - directions[i][1] * step;
            if (newX >= 0 && newX < BOARD_SIZE && newY >= 0 && newY < BOARD_SIZE && board[newX][newY] == player) {
                count++;
            } else {
                break;
            }
        }

        // 如果在任意方向上達到五子，則返回勝利
        if (count >= 5) {
            return true;
        }
    }

    // 如果沒有找到五子連線，返回 false
    return false;
}
