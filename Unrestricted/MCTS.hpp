#ifndef MCTS_HPP
#define MCTS_HPP
#include <random>

#include "Node.hpp"
struct Node;
struct BoardScore;
class MCTS {
   public:
    MCTS(int simTimes, int numThreads);   // 構造函數聲明
    int run(Node* root, int iterations);  // run 方法聲明
    Node* expansion(Node* node);          // expansion 方法聲明

   private:
    int numThreads;
    const double COEFFICIENT = 1.414;
    const int MAX_DEEP = 50;
    int simulationTimes;
    inline static const Position direction[8] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}, {-1, 0}, {0, -1}, {-1, -1}, {-1, 1}};

    std::mt19937 generator;
    Node* selection(Node* node);
    void backpropagation(Node* node, Node* endNode, bool isXTurn, double win);
    int playout(Node* node);
    double parallelPlayouts(int thread, int simulationTimes, Node* node);
};

#endif