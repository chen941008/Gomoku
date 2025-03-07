#include "Game.hpp"

#include <stdint.h>

#include <iomanip>
#include <iostream>

#include "MCTS.hpp"
#include "Node.hpp"
#define RED "\033[31;1m"      // 亮紅色
#define GREEN "\033[32;1m"    // 亮綠色
#define YELLOW "\033[33;1m"   // 亮黃色
#define BLUE "\033[34;1m"     // 亮藍色
#define MAGENTA "\033[35;1m"  // 亮洋紅色
#define CYAN "\033[36;1m"     // 亮青色
#define DEFAULT "\033[0m"     // 重置為預設

// Function declaration for expansion

using namespace std;
void Game::startGame() {
    Node* root = new Node();
    // generateFullTree(root);
    Node* currentNode = root;  // CurrentNode為當前棋盤最後一個子的節點，會去選擇他的子節點來下棋
    int playerOrder, currentOrder = 0, aiMode, iterationTimes, simulationTimes;
    cout << "Input stimulation times." << endl;
    cin >> simulationTimes;
    MCTS ai(simulationTimes, 6);
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
    uint64_t boardBlack[BITBOARD_COUNT];
    uint64_t boardWhite[BITBOARD_COUNT];
    memset(boardBlack, 0, sizeof(boardBlack));
    memset(boardWhite, 0, sizeof(boardWhite));
    boardBlack[3] = 0xFFFFFFFE00000000;  // 沒用到的bit直接賦值為1=佔據
    boardWhite[3] = 0xFFFFFFFE00000000;  // 沒用到的bit直接賦值為1=佔據
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
            printBoard(boardBlack, boardWhite, currentNode->lastMove);
            break;
        }
        printBoard(boardBlack, boardWhite, currentNode->lastMove);
        if (currentOrder % 2 == playerOrder) {  // Player turn
            showEachNodeInformation(currentNode);
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
                if (getBit(boardBlack, {X, Y}) || getBit(boardWhite, {X, Y})) {
                    cout << "This position is already taken" << endl;
                    continue;
                }
                break;
            }
            if (currentOrder % 2 == 0) {
                setBit(boardBlack, {X, Y});
            } else {
                setBit(boardWhite, {X, Y});
            }
            if (currentOrder >= CHECKWIN_THRESHOLD && checkWin({X, Y}, boardBlack, boardWhite, currentOrder % 2 == 0)) {
                cout << "You win" << endl;
                printBoard(boardBlack, boardWhite, currentNode->lastMove);
                break;
            }
            bool found = false;
            for (int i = 0; i < MAX_CHILDREN && currentNode->children[i] != nullptr; i++) {
                if (currentNode->children[i]->lastMove.x == X && currentNode->children[i]->lastMove.y == Y) {
                    currentNode = currentNode->children[i];
                    found = true;
                    break;
                }
            }
            // 如果玩家的落子不在拓展的節點中，則新建一個節點並加入
            if (!found) {
                Node* newNode = new Node({X, Y}, currentNode);
                currentNode->children[0] = newNode;
                currentNode = newNode;
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
            if (currentOrder == 0) {
                Node* newNode = new Node({7, 7}, currentNode);
                currentNode->children[0] = newNode;
                currentNode = newNode;
                setBit(boardBlack, {7, 7});
                cout << "AI choose " << "7" << " " << "7" << endl;
                currentOrder++;
                continue;
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
                setBit(boardBlack, lastMove);
            } else {
                setBit(boardWhite, lastMove);
            }
            showEachNodeInformation(currentNode);
            cout << "AI choose " << lastMove.x << " " << lastMove.y << endl;
            currentNode = bestChild;
            if (currentOrder >= CHECKWIN_THRESHOLD &&
                checkWin(lastMove, boardBlack, boardWhite, currentOrder % 2 == 0)) {
                cout << "AI win" << endl;
                printBoard(boardBlack, boardWhite, currentNode->lastMove);
                break;
            }
        }
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

void Game::printBoard(uint64_t* boardBlack, uint64_t* boardWhite, Position lastMove) {
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
            if (getBit(boardBlack, {i, j})) {
                if (i == lastMove.x && j == lastMove.y) {
                    cout << RED << " X " << DEFAULT;
                } else {
                    cout << YELLOW << " X " << DEFAULT;
                }
            } else if (getBit(boardWhite, {i, j})) {
                if (i == lastMove.x && j == lastMove.y) {
                    cout << RED << " O " << DEFAULT;
                } else {
                    cout << CYAN << " O " << DEFAULT;
                }
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
// 檢查某個方向是否有連續5顆棋子
bool Game::checkDirection(Position lastMove, Position direction, uint64_t* board) {
    int count = 1;  // 目前這顆棋子算1個
    int x = lastMove.x, y = lastMove.y;
    int dx = direction.x, dy = direction.y;
    for (int i = 1; i < 5; i++) {
        int nx = x + dx * i, ny = y + dy * i;
        if (nx < 0 || ny < 0 || nx >= BOARD_SIZE || ny >= BOARD_SIZE || !getBit(board, {nx, ny})) break;
        count++;
    }
    for (int i = 1; i < 5; i++) {
        int nx = x - dx * i, ny = y - dy * i;
        if (nx < 0 || ny < 0 || nx >= BOARD_SIZE || ny >= BOARD_SIZE || !getBit(board, {nx, ny})) break;
        count++;
    }
    return count >= 5;
}

// 總體檢查是否勝利
bool Game::checkWin(Position lastMove, uint64_t boardBlack[BITBOARD_COUNT], uint64_t boardWhite[BITBOARD_COUNT],
                    bool isBlackTurn) {
    static const Position directions[4] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};

    uint64_t* targetBoard = isBlackTurn ? boardBlack : boardWhite;

    return checkDirection(lastMove, directions[0], targetBoard) ||  // 水平
           checkDirection(lastMove, directions[1], targetBoard) ||  // 垂直
           checkDirection(lastMove, directions[2], targetBoard) ||  // 斜對角（\）
           checkDirection(lastMove, directions[3], targetBoard);    // 斜對角（/）
}
void Game::showEachNodeInformation(Node* currentNode) {
    for (int i = 0; i < MAX_CHILDREN && currentNode->children[i] != nullptr; i++) {
        // 設定固定格式與寬度
        std::cout << std::fixed << std::setprecision(3) << "move: " << std::setw(3)
                  << currentNode->children[i]->lastMove.x << " " << std::setw(3) << currentNode->children[i]->lastMove.y
                  << " | wins: " << std::setw(8) << currentNode->children[i]->wins << " | visits: " << std::setw(8)
                  << currentNode->children[i]->visits << " | winRate: " << std::setw(8)
                  << (currentNode->children[i]->wins / currentNode->children[i]->visits) << std::endl;
    }
}