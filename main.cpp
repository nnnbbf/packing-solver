#include "solver.hpp"
#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>

int main(int argc, char* argv[]) {
    std::string inputFile = "example4.json";
    
    if (argc > 1) {
        inputFile = argv[1];
    }
    
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "    二维布局问题求解器 v2.0 (jsoncpp)   \n";
    std::cout << "========================================\n\n";
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    PackingSolver solver;
    
    std::cout << "📁 加载输入文件: " << inputFile << std::endl;
    if (!solver.loadFromFile(inputFile)) {
        std::cerr << "❌ 错误: 加载输入文件失败" << std::endl;
        return 1;
    }
    
    // 打印输入统计
    solver.printStats();
    
    std::cout << "🔍 开始求解..." << std::endl;
    bool success = solver.solve();
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    if (success) {
        std::cout << "\n✅ 找到可行布局方案！" << std::endl;
        solver.printResult();
    } else {
        std::cout << "\n❌ 未找到可行布局方案" << std::endl;
    }
    
    std::cout << "\n⏱️  求解时间: " << duration.count() << " ms" << std::endl;
    std::cout << "========================================\n";
    
    return 0;
}