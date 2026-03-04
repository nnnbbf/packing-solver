#ifndef SOLVER_HPP
#define SOLVER_HPP

#include "layout.hpp"
#include <fstream>
#include <sstream>
#include <map>
#include <jsoncpp/json/json.h>

class PackingSolver {
private:
    std::vector<Point> contour;
    Door door;
    std::vector<Item> items;
    std::unique_ptr<LayoutSolver> solver;
    
    ItemType stringToItemType(const std::string& type) {
        if (type == "fridge") return ItemType::FRIDGE;
        if (type == "shelf") return ItemType::SHELF;
        if (type == "overShelf") return ItemType::OVER_SHELF;
        if (type == "iceMaker") return ItemType::ICE_MAKER;
        
        // 根据名称中的关键字判断
        if (type.find("fridge") != std::string::npos) return ItemType::FRIDGE;
        if (type.find("shelf") != std::string::npos) return ItemType::SHELF;
        if (type.find("overShelf") != std::string::npos) return ItemType::OVER_SHELF;
        if (type.find("iceMaker") != std::string::npos) return ItemType::ICE_MAKER;
        
        throw std::runtime_error("未知物品类型: " + type);
    }
    
public:
    bool loadFromFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "无法打开文件: " << filename << std::endl;
            return false;
        }
        
        Json::Value root;
        Json::CharReaderBuilder reader;
        std::string errs;
        
        if (!Json::parseFromStream(reader, file, &root, &errs)) {
            std::cerr << "JSON解析错误: " << errs << std::endl;
            return false;
        }
        
        file.close();
        
        // 清空现有数据
        contour.clear();
        items.clear();
        
        try {
            // 1. 解析边界轮廓
            if (root.isMember("boundary") && root["boundary"].isArray()) {
                const Json::Value& boundary = root["boundary"];
                for (const auto& point : boundary) {
                    if (point.isArray() && point.size() >= 2) {
                        double x = point[0].asDouble();
                        double y = point[1].asDouble();
                        contour.emplace_back(x, y);
                    }
                }
                std::cout << "解析到 " << contour.size() << " 个轮廓点\n";
            }
            
            // 计算轮廓中心（用于内开门空间计算）
            Point contourCenter(0, 0);
            if (!contour.empty()) {
                for (const auto& p : contour) {
                    contourCenter.x += p.x;
                    contourCenter.y += p.y;
                }
                contourCenter.x /= contour.size();
                contourCenter.y /= contour.size();
            }
            
            // 2. 解析门
            if (root.isMember("door") && root["door"].isArray()) {
                const Json::Value& doorArray = root["door"];
                if (doorArray.size() >= 2) {
                    // 第一个点
                    const Json::Value& p1Array = doorArray[0];
                    // 第二个点
                    const Json::Value& p2Array = doorArray[1];
                    
                    if (p1Array.isArray() && p1Array.size() >= 2 &&
                        p2Array.isArray() && p2Array.size() >= 2) {
                        
                        Point p1(p1Array[0].asDouble(), p1Array[1].asDouble());
                        Point p2(p2Array[0].asDouble(), p2Array[1].asDouble());
                        
                        // 解析是否内开门
                        bool isInward = false;
                        if (root.isMember("isOpenInward")) {
                            isInward = root["isOpenInward"].asBool();
                        }
                        
                        // 使用带轮廓中心的构造函数，正确计算内开门 N×N 区域
                        door = Door(p1, p2, isInward, contourCenter);
                        
                        std::cout << "解析到门: " << p1 << " -> " << p2 
                                  << (isInward ? " (内开门)" : " (外开门)") << "\n";
                    }
                }
            }
            
            // 3. 解析物品
            if (root.isMember("algoToPlace") && root["algoToPlace"].isObject()) {
                const Json::Value& algoToPlace = root["algoToPlace"];
                const auto& memberNames = algoToPlace.getMemberNames();
                
                for (const auto& name : memberNames) {
                    const Json::Value& dimensions = algoToPlace[name];
                    if (dimensions.isArray() && dimensions.size() >= 2) {
                        double length = dimensions[0].asDouble();
                        double width = dimensions[1].asDouble();
                        
                        // 根据名称确定类型
                        items.emplace_back(name, stringToItemType(name), length, width);
                    }
                }
                
                std::cout << "解析到 " << items.size() << " 个物品\n";
            }
            
        } catch (const std::exception& e) {
            std::cerr << "数据解析错误: " << e.what() << std::endl;
            return false;
        }
        
        return !contour.empty() && !items.empty();
    }
    
    bool solve() {
        solver = std::make_unique<LayoutSolver>(contour, door);
        
        for (const auto& item : items) {
            solver->addItem(item);
        }
        
        return solver->solve();
    }
    
    void printResult() const {
        if (solver) {
            solver->printSolution();
        } else {
            std::cout << "没有可用的解决方案\n";
        }
    }
    
    // 获取详细统计信息
    void printStats() const {
        std::cout << "\n========== 输入数据统计 ==========\n";
        std::cout << "轮廓点数: " << contour.size() << "\n";
        
        // 计算轮廓包围盒
        if (!contour.empty()) {
            double minX = contour[0].x, maxX = contour[0].x;
            double minY = contour[0].y, maxY = contour[0].y;
            for (const auto& p : contour) {
                minX = std::min(minX, p.x);
                maxX = std::max(maxX, p.x);
                minY = std::min(minY, p.y);
                maxY = std::max(maxY, p.y);
            }
            std::cout << "轮廓范围: X[" << minX << ", " << maxX << "] "
                      << "Y[" << minY << ", " << maxY << "]\n";
            std::cout << "轮廓面积: " << (maxX - minX) * (maxY - minY) << "\n";
        }
        
        std::cout << "\n门信息:\n";
        std::cout << "  位置: " << door.p1 << " -> " << door.p2 << "\n";
        std::cout << "  类型: " << (door.isInward ? "内开门" : "外开门") << "\n";
        std::cout << "  宽度: " << door.width << "\n";
        
        std::cout << "\n物品列表:\n";
        int fridgeCount = 0, shelfCount = 0, overShelfCount = 0, iceMakerCount = 0;
        for (const auto& item : items) {
            std::cout << "  " << item.name << ": "
                      << item.length << "x" << item.width << " ["
                      << itemTypeToString(item.type) << "]\n";
            
            if (item.isFridge()) fridgeCount++;
            else if (item.isShelf()) shelfCount++;
            else if (item.isOverShelf()) overShelfCount++;
            else if (item.isIceMaker()) iceMakerCount++;
        }
        
        std::cout << "\n物品统计:\n";
        std::cout << "  冰箱: " << fridgeCount << "\n";
        std::cout << "  货架: " << shelfCount << "\n";
        std::cout << "  离地架: " << overShelfCount << "\n";
        std::cout << "  制冰机: " << iceMakerCount << "\n";
        std::cout << "  总计: " << items.size() << " 个物品\n";
        std::cout << "================================\n\n";
    }
};

#endif // SOLVER_HPP