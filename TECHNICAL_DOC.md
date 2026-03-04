# 二维布局问题求解器 - 技术文档

## 1. 项目概述

### 1.1 问题定义

在给定的不规则多边形轮廓内，放置多个矩形物品（冰箱、货架、离地架、制冰机），满足以下约束：
- 所有物品必须完全位于轮廓内
- 物品之间不能重叠
- 物品可旋转，但需与轮廓边平行或垂直（优先直角边）
- 物品不能遮挡门的位置
- 内开门会占据 N×N 的空间（N 为门宽）
- 冰箱的开门边前方不能放置其他物品

### 1.2 技术栈

- **编程语言**: C++11/14/17
- **依赖库**: jsoncpp
- **核心算法**: 贪心 + 启发式搜索 + 分离轴定理(SAT)

---

## 2. 文件结构

```
packing-solver/
├── main.cpp          # 主程序入口
├── solver.hpp        # 输入解析和流程控制
├── layout.hpp        # 核心布局算法
├── geometry.hpp      # 几何基础结构
├── input.json        # 默认输入文件
├── example1-4.json   # 测试用例
└── build.sh          # 编译脚本
```

---

## 3. 核心算法流程

### 3.1 调用链

```
main()
    │
    ├─ PackingSolver::loadFromFile()     # 解析 JSON 输入
    │
    ├─ PackingSolver::printStats()       # 打印输入统计
    │
    ├─ PackingSolver::solve()            # 开始求解
    │       │
    │       ├─ LayoutSolver 构造函数
    │       │       └─ analyzeWalls()    # 分析可用墙边
    │       │
    │       └─ LayoutSolver::solve()      # 核心求解
    │               │
    │               ├─ 物品排序（冰箱优先，面积降序）
    │               │
    │               └─ for 每个物品:
    │                   │
    │                   ├─ tryPlaceAgainstWall()  # 贴墙
    │                   ├─ tryPlaceInCorner()    # 角落
    │                   └─ tryPlaceInGrid()      # 网格搜索
    │
    └─ PackingSolver::printResult()      # 输出结果
```

### 3.2 放置策略优先级

```
1. 贴墙放置 (tryPlaceAgainstWall)
   - 沿轮廓边线性搜索
   - 优先使用直角边（非斜边）
   - 物体角度与墙边平行或垂直

2. 角落放置 (tryPlaceInCorner)
   - 利用多边形内角点
   - 尝试四种对齐方式

3. 网格搜索 (tryPlaceInGrid)
   - 自适应网格步长
   - 最后 fallback 方案
```

---

## 4. 关键算法说明

### 4.1 几何计算

#### 点在多边形内检测（射线法）
```cpp
bool pointInPolygon(Point p) {
    // 从点出发发射水平射线
    // 统计与多边形边的交点数
    // 奇数点在内部，偶数点在外部
}
```

#### 矩形碰撞检测（分离轴定理 SAT）
```cpp
bool rectanglesOverlap(Rectangle r1, Rectangle r2) {
    // 获取所有边法线作为分离轴
    // 在每个轴上投影
    // 存在分离轴则不相交
}
```

### 4.2 门避让规则

#### 外开门
- 检查物品是否与门线段相交
- 检查物品顶点是否在门线段附近（安全距离 100mm）

#### 内开门
- 计算 N×N 正方形区域（N = 门宽）
- 区域中心位于室内侧（根据轮廓中心判断）
- 物品不能进入该区域

### 4.3 冰箱开门边避让

```cpp
bool checkFridgeDoorClearance(fridge, other) {
    // 获取冰箱开门边（length 边）
    Line doorEdge = fridge.getDoorEdge();
    
    // 计算开门方向（垂直于开门边向外）
    // 开门需要至少 width 的 clearance
    
    // 检查 other 是否在开门边前方的 clearance 范围内
}
```

### 4.4 角度对齐规则（重要）

**核心原则**：物体优先与直角边对齐

1. **直角边优先**：先遍历水平边和垂直边
2. **角度计算**：
   - 水平边 → 物体角度 0° 或 90°
   - 垂直边 → 物体角度 0° 或 90°
   - 斜边 → 物体旋转到与该边平行或垂直

```cpp
// 墙边角度计算
double wallAngle = wall.getAngle();  // 墙边与 X 轴夹角

// 物体可选角度
double angle1 = wallAngle;           // 与墙边平行
double angle2 = wallAngle + 90;      // 与墙边垂直

// 直接采用墙边角度，确保物体边与墙边严格平行或垂直，无需四舍五入
```

---

## 5. 约束检查

### 5.1 综合验证

```cpp
bool validatePlacement(Rectangle rect) {
    // 1. 轮廓内检查（所有顶点）
    if (!rectangleInPolygon(rect)) return false;
    
    // 2. 碰撞检测（SAT）
    for (placed : placedItems) {
        if (rectanglesOverlap(rect, placed)) return false;
    }
    
    // 3. 门冲突检查
    if (checkDoorConflict(rect)) return false;
    
    // 4. 冰箱开门边避让
    if (checkFridgeDoorClearance(placedFridge, rect)) return false;
    
    return true;
}
```

### 5.2 约束优先级

| 约束 | 类型 | 说明 |
|------|------|------|
| 轮廓边界 | 硬约束 | 所有顶点必须在多边形内 |
| 物品碰撞 | 硬约束 | 使用 SAT 精确检测 |
| 门避让 | 硬约束 | 门区域及 N×N 禁区 |
| 冰箱开门边 | 硬约束 | 开门方向禁止放置 |
| 货架重叠 | 硬约束 | 货架不允许重叠 |

---

## 6. 物品类型规则

| 类型 | 关键字 | 特殊属性 |
|------|--------|----------|
| 冰箱 | fridge | length 边为开门边，开门边前方禁止放置物品 |
| 货架 | shelf | 普通矩形，不允许重叠 |
| 离地架 | overShelf | 普通矩形 |
| 制冰机 | iceMaker | 普通矩形 |

---

## 7. 输入输出格式

### 7.1 输入 (JSON)

```json
{
  "boundary": [[x1,y1], [x2,y2], ...],  // 轮廓顶点（首尾相连）
  "door": [[x1,y1], [x2,y2]],           // 门两端点
  "isOpenInward": true/false,            // 是否内开门
  "algoToPlace": {
    "物品名": [length, width],
    ...
  }
}
```

### 7.2 输出

```
物品名 [类型]
  中心点: (x, y)
  旋转角度: n°
  开门边: (x1,y1) -> (x2,y2)  // 仅冰箱

总放置面积: xxx
空间利用率: xx%
```

---

## 8. 编译与运行

### 8.1 编译

```bash
# C++11（推荐）
g++ -std=c++11 -O2 -o solver main.cpp -ljsoncpp

# C++14/17
g++ -std=c++17 -O2 -o solver main.cpp -ljsoncpp
```

### 8.2 运行

```bash
# 默认输入
./solver

# 指定输入文件
./solver input.json
./solver example1.json
./solver example2.json
./solver example3.json
./solver example4.json
```

---

## 9. AI 工具使用说明

### 9.1 使用的 AI 工具

- **GitHub Copilot**: 代码补全
- **Claude/ChatGPT**: 算法思路、Debug
- **Qoder**: 代码审查

### 9.2 AI 帮助的部分

- ✅ 思路拆解：几何检测、放置策略、约束检查模块化
- ✅ 代码生成：SAT、射线法等标准算法
- ✅ Debug：识别内开门计算、冰箱开门边缺失
- ✅ 文档编写

### 9.3 人工完成的关键逻辑

- 整体架构设计
- 放置策略优先级确定
- 直角边优先对齐算法
- 约束条件权衡

---

## 10. 测试示例

### Example 1: 不规则多边形
- 轮廓：7 个顶点的不规则形状
- 门：外开门
- 物品：冰箱、货架、离地架、制冰机

### Example 2: 矩形房间
- 轮廓：简单矩形
- 门：外开门
- 物品：多个货架

### Example 3: L 型空间
- 轮廓：L 型多边形
- 门：内开门
- 物品：冰箱、多个货架

### Example 4: 带凹槽矩形
- 轮廓：矩形带凹槽
- 门：外开门
- 物品：冰箱、货架、离地架

---

**版本**: v2.0  
**更新日期**: 2026-03-03
