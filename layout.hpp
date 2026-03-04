#ifndef LAYOUT_HPP
#define LAYOUT_HPP

#include "geometry.hpp"
#include <vector>
#include <memory>
#include <set>
#include <map>
#include <queue>
#include <functional>
#include <random>

struct Door {
    Point p1, p2;
    bool isInward;  // 是否为内开门
    double width;   // 门宽度
    Point occupiedCenter;  // 内开门 N×N 区域的中心

    Door() : p1(0, 0), p2(0, 0), isInward(false), width(0), occupiedCenter(0, 0) {}
    
    Door(const Point& p1_, const Point& p2_, bool inward = false)
        : p1(p1_), p2(p2_), isInward(inward) {
        width = p1.distanceTo(p2);
        occupiedCenter = getCenter();  // 默认使用门中点
    }
    
    // 构造函数（带轮廓中心，用于计算内开门区域）
    Door(const Point& p1_, const Point& p2_, bool inward, const Point& contourCenter)
        : p1(p1_), p2(p2_), isInward(inward) {
        width = p1.distanceTo(p2);
        
        if (isInward) {
            // 计算 N×N 区域的中心：在室内侧，距离门 width/2
            Line doorLine(p1, p2);
            Point normal = doorLine.getInwardNormal(contourCenter);
            
            // 归一化
            double normLen = std::sqrt(normal.x*normal.x + normal.y*normal.y);
            if (normLen > EPSILON) {
                normal.x /= normLen;
                normal.y /= normLen;
            }
            
            // 从门中点向室内偏移 width/2
            Point mid = getCenter();
            occupiedCenter = Point(
                mid.x + normal.x * (width / 2),
                mid.y + normal.y * (width / 2)
            );
        } else {
            occupiedCenter = getCenter();
        }
    }

    Line getLine() const { return Line(p1, p2); }

    // 获取门中点
    Point getCenter() const {
        return Point((p1.x + p2.x) / 2, (p1.y + p2.y) / 2);
    }

    // 获取门所在的墙
    Line getWall() const {
        return Line(p1, p2);
    }

    // 检查点是否在门区域内（用于禁区检测）
    bool isPointInDoorArea(const Point& p, double margin = 0) const {
        // 对于外开门，只需要检查是否在门线段上
        if (!isInward) {
            // 计算点到线段的距离
            Line doorLine = getLine();
            Point dir = doorLine.getDirection();
            double len = dir.x * dir.x + dir.y * dir.y;

            if (len < EPSILON) return false;

            double t = ((p.x - p1.x) * dir.x + (p.y - p1.y) * dir.y) / len;
            if (t < 0 || t > 1) return false;

            Point proj(p1.x + t * dir.x, p1.y + t * dir.y);
            return p.distanceTo(proj) < margin + EPSILON;
        }
        else {
            // 内开门需要检查 N×N 正方形区域（N 为门宽）
            // occupiedCenter 已经在构造函数中计算为室内侧的点
            return std::abs(p.x - occupiedCenter.x) <= width / 2 + margin &&
                std::abs(p.y - occupiedCenter.y) <= width / 2 + margin;
        }
    }
};

// 可用墙边空间
struct WallSpace {
    Point start;        // 起点
    Point end;          // 终点
    double length;      // 可用长度
    Line wall;          // 对应的墙
    bool isHorizontal;  // 是否水平
    bool isVertical;    // 是否垂直（直角边）
    bool isRightAngle;  // 是否为直角边（水平或垂直）
    double wallAngle;   // 墙边角度（度）
    Point direction;    // 方向向量
    Point inwardNormal; // 指向内部的法向量

    WallSpace(const Point& s, const Point& e, const Line& w, const Point& normal)
        : start(s), end(e), wall(w) {
        length = s.distanceTo(e);
        isHorizontal = w.isHorizontal();
        isVertical = w.isVertical();
        isRightAngle = isHorizontal || isVertical;
        wallAngle = w.getAngle();
        direction = w.getDirection();
        double len = std::sqrt(direction.x * direction.x + direction.y * direction.y);
        if (len > EPSILON) {
            direction.x /= len;
            direction.y /= len;
        }
        inwardNormal = normal;
    }
};

class LayoutSolver {
private:
    std::vector<Point> contour;        // 轮廓点
    Door door;                          // 门
    std::vector<Item> items;            // 待放置物品
    std::vector<Rectangle> placed;      // 已放置物品
    Point contourCenter;                 // 轮廓中心
    double minX, maxX, minY, maxY;       // 轮廓包围盒

    std::vector<WallSpace> wallSpaces;  // 可用的墙边空间
    std::vector<Line> edges;             // 轮廓边

    // 几何计算辅助函数
    double crossProduct(const Point& O, const Point& A, const Point& B) const {
        return (A.x - O.x) * (B.y - O.y) - (A.y - O.y) * (B.x - O.x);
    }

    bool onSegment(const Point& p, const Point& q, const Point& r) const {
        if (q.x <= std::max(p.x, r.x) + EPSILON &&
            q.x >= std::min(p.x, r.x) - EPSILON &&
            q.y <= std::max(p.y, r.y) + EPSILON &&
            q.y >= std::min(p.y, r.y) - EPSILON)
            return true;
        return false;
    }

    

    bool segmentsIntersect(const Line& l1, const Line& l2) const {
        double o1 = crossProduct(l1.p1, l1.p2, l2.p1);
        double o2 = crossProduct(l1.p1, l1.p2, l2.p2);
        double o3 = crossProduct(l2.p1, l2.p2, l1.p1);
        double o4 = crossProduct(l2.p1, l2.p2, l1.p2);

        // 一般情况
        if (o1 * o2 < -EPSILON && o3 * o4 < -EPSILON)
            return true;

        // 特殊情况：共线且重叠
        if (std::abs(o1) < EPSILON && onSegment(l1.p1, l2.p1, l1.p2)) return true;
        if (std::abs(o2) < EPSILON && onSegment(l1.p1, l2.p2, l1.p2)) return true;
        if (std::abs(o3) < EPSILON && onSegment(l2.p1, l1.p1, l2.p2)) return true;
        if (std::abs(o4) < EPSILON && onSegment(l2.p1, l1.p2, l2.p2)) return true;

        return false;
    }

    bool pointInPolygon(const Point& p) const {
        int n = contour.size();
        bool inside = false;

        for (int i = 0, j = n - 1; i < n; j = i++) {
            if (((contour[i].y > p.y) != (contour[j].y > p.y)) &&
                (p.x < (contour[j].x - contour[i].x) * (p.y - contour[i].y) /
                    (contour[j].y - contour[i].y) + contour[i].x)) {
                inside = !inside;
            }
        }

        return inside;
    }

    bool rectangleInPolygon(const Rectangle& rect) const {
        auto vertices = rect.getVertices();

        // 检查所有顶点是否都在多边形内
        for (const auto& v : vertices) {
            if (!pointInPolygon(v)) {
                return false;
            }
        }

        return true;
    }

    // 分离轴定理检测矩形重叠
    bool rectanglesOverlap(const Rectangle& r1, const Rectangle& r2) const {
        auto vertices1 = r1.getVertices();
        auto vertices2 = r2.getVertices();

        // 获取所有可能的分离轴（矩形的边法线）
        std::vector<Point> axes;

        auto addEdgeNormals = [&axes](const std::vector<Point>& verts) {
            for (int i = 0; i < 4; i++) {
                Point edge = verts[(i + 1) % 4] - verts[i];
                Point normal(-edge.y, edge.x);  // 边的法线
                double len = std::sqrt(normal.x * normal.x + normal.y * normal.y);
                if (len > EPSILON) {
                    normal.x /= len;
                    normal.y /= len;
                }
                axes.push_back(normal);
            }
            };

        addEdgeNormals(vertices1);
        addEdgeNormals(vertices2);

        // 在每个轴上进行投影检测
        for (const auto& axis : axes) {
            double min1 = std::numeric_limits<double>::max();
            double max1 = -std::numeric_limits<double>::max();
            double min2 = std::numeric_limits<double>::max();
            double max2 = -std::numeric_limits<double>::max();

            for (const auto& v : vertices1) {
                double proj = v.dot(axis);
                min1 = std::min(min1, proj);
                max1 = std::max(max1, proj);
            }

            for (const auto& v : vertices2) {
                double proj = v.dot(axis);
                min2 = std::min(min2, proj);
                max2 = std::max(max2, proj);
            }

            // 允许边界相切（touching）被视为不相交：使用 <= 并加上容差
            if (max1 <= min2 + EPSILON || max2 <= min1 + EPSILON) {
                return false;  // 存在分离轴，不相交（允许相切）
            }
        }

        return true;  // 所有轴都重叠
    }
    
    // 检查冰箱开门边是否与 other 矩形冲突
    bool checkFridgeDoorClearance(const Rectangle& fridge, const Rectangle& other) const {
        // 获取冰箱开门边
        Line doorEdge = fridge.getDoorEdge();
        
        // 计算开门方向（垂直于开门边向外）
        Point doorDir = doorEdge.getDirection();
        // 归一化
        double len = std::sqrt(doorDir.x*doorDir.x + doorDir.y*doorDir.y);
        if (len > EPSILON) {
            doorDir.x /= len;
            doorDir.y /= len;
        }
        
        // 开门边的法线（指向外部）
        Point normal(-doorDir.y, doorDir.x);
        
        // 开门需要的 clearance 距离（至少为冰箱宽度）
        double clearanceDist = fridge.width;
        
        // 检查 other 的所有顶点是否在开门边的前方 clearanceDist 范围内
        auto vertices = other.getVertices();
        for (const auto& v : vertices) {
            // 计算 v 相对于开门边的位置
            Point toV = v - doorEdge.p1;
            
            // 如果在开门方向上（法线方向为正）
            double distAlongNormal = toV.dot(normal);
            
            if (distAlongNormal > -EPSILON) {  // 在开门方向或边界上
                // 计算到开门边的垂直距离
                Point projection = doorEdge.p1 + doorDir * (toV.dot(doorDir));
                double perpendicularDist = v.distanceTo(projection);
                
                // 如果距离过近，视为冲突
                if (perpendicularDist < clearanceDist - EPSILON) {
                    return true;
                }
            }
        }
        
        return false;  // 无冲突
    }

    bool checkDoorConflict(const Rectangle& rect) const {
        auto vertices = rect.getVertices();

        // 检查矩形的任何点是否在门区域内
        double margin = 100;  // 安全距离
        for (const auto& v : vertices) {
            if (door.isPointInDoorArea(v, margin)) {
                return true;
            }
        }

        // 检查矩形的边是否与门线段相交
        Line doorLine = door.getLine();
        auto edges = rect.getEdges();
        for (const auto& edge : edges) {
            if (segmentsIntersect(edge, doorLine)) {
                return true;
            }
        }

        return false;
    }

    bool validatePlacement(const Rectangle& rect, const Item* currentItem = nullptr) {
        // 检查是否在轮廓内
        if (!rectangleInPolygon(rect)) {
            return false;
        }

        // 检查是否与已放置物品重叠
        for (size_t i = 0; i < placed.size(); i++) {
            const auto& placedRect = placed[i];
            
            // 所有物品（包括货架）都不允许重叠
            if (rectanglesOverlap(rect, placedRect)) {
                return false;
            }
            
            // 特殊规则：检查冰箱开门边避让
            // 如果已放置的是冰箱，检查当前物品是否在其开门边前方
            if (i < items.size() && items[i].isFridge()) {
                if (checkFridgeDoorClearance(placedRect, rect)) {
                    return false;
                }
            }
        }

        // 检查是否与门冲突
        if (checkDoorConflict(rect)) {
            return false;
        }

        return true;
    }

    void analyzeWalls() {
        wallSpaces.clear();
        edges.clear();

        // 分离直角边和斜边
        std::vector<WallSpace> rightAngleSpaces;
        std::vector<WallSpace> obliqueSpaces;

        int n = contour.size();
        for (int i = 0; i < n; i++) {
            Point p1 = contour[i];
            Point p2 = contour[(i + 1) % n];
            Line edge(p1, p2);
            edges.push_back(edge);

            // 获取指向内部的法线
            Point inwardNormal = edge.getInwardNormal(contourCenter);

            // 检查是否是门所在的边
            Line doorLine = door.getLine();
            if (!segmentsIntersect(edge, doorLine)) {
                double length = edge.length();

                // 跳过太短的边
                if (length < 500) continue;

                WallSpace ws(p1, p2, edge, inwardNormal);

                // 优先添加直角边
                if (ws.isRightAngle) {
                    rightAngleSpaces.push_back(ws);
                    std::cout << "发现直角墙边: 从 " << p1 << " 到 " << p2
                        << " 长度=" << length << " (水平=" << ws.isHorizontal << ", 垂直=" << ws.isVertical << ")";
                } else {
                    obliqueSpaces.push_back(ws);
                    std::cout << "发现斜墙边: 从 " << p1 << " 到 " << p2
                        << " 角度=" << ws.wallAngle << "°\n";
                }
            }
        }

        // 直角边优先
        wallSpaces = rightAngleSpaces;
        wallSpaces.insert(wallSpaces.end(), obliqueSpaces.begin(), obliqueSpaces.end());
    }

    // 将角度四舍五入到最近的 90° 倍数
    double roundToNearest90(double angle) const {
        // 先归一化到 [0, 360)
        while (angle < 0) angle += 360;
        while (angle >= 360) angle -= 360;
        
        // 四舍五入到最近的 90° 倍数
        double nearest = std::round(angle / 90.0) * 90.0;
        
        // 处理 360° 的情况
        if (std::abs(nearest - 360) < EPSILON) nearest = 0;
        
        return nearest;
    }

    // 根据墙边角度计算物体的对齐角度
    double getItemAngleForWall(const WallSpace& space, int rot) const {
        // 无论直角边还是斜边，均直接采用墙边角度
        double baseAngle = space.wallAngle;
        double angle1 = baseAngle;         // 与墙边平行
        double angle2 = baseAngle + 90.0;  // 与墙边垂直
        return (rot == 0) ? angle1 : angle2;
    }

    bool tryPlaceAgainstWall(Item& item, const WallSpace& space) {
        // 尝试两种旋转方向
        for (int rot = 0; rot <= 1; rot++) {
            // 计算物体的对齐角度（平行或垂直于墙）
            double itemAngle = getItemAngleForWall(space, rot);

            // 根据 rot 决定哪一边沿墙方向（rot==0 -> length 沿墙，rot==1 -> width 沿墙）
            double placeLength = (rot == 0) ? item.length : item.width;
            double placeWidth  = (rot == 0) ? item.width  : item.length;

            // 沿墙方向需要的长度
            double required = placeLength;

            if (space.length < required - EPSILON) {
                continue;
            }

            // 使用墙方向向量精确计算沿墙位置（支持斜墙）
            // 步长根据物体较短边动态调整，便于贴合
            double wallStep = std::max(10.0, std::min(placeLength, placeWidth) / 4.0);
            for (double offset = 0; offset <= space.length - required + EPSILON; offset += wallStep) {
                Point basePos = space.start + space.direction * (offset + required / 2.0);

                // 向内部偏移半个宽度（inwardNormal 已归一化）
                Point center = basePos + space.inwardNormal * (placeWidth / 2.0);

                // 创建候选矩形（使用计算出的对齐角度）
                Rectangle candidate(center, item.length, item.width, itemAngle);
                
                if (validatePlacement(candidate, &item)) {
                    item.placement = candidate;
                    item.placed = true;
                    placed.push_back(candidate);
                    std::cout << "  放置成功：" << item.name
                        << " 在 " << center
                        << " 旋转=" << itemAngle << "°\n";
                    return true;
                }
            }
        }

        return false;
    }

    // 智能网格搜索（适应大坐标）
    bool tryPlaceInGrid(Item& item) {
        // 根据物品尺寸动态调整网格步长（更细的步长以便紧贴）
        double step = std::min(item.length, item.width) / 3.0;
        step = std::max(step, 20.0);  // 最小步长20

        // 尝试两种旋转
        for (int rot = 0; rot <= 1; rot++) {
            double currentLen = (rot == 0) ? item.length : item.width;
            double currentWid = (rot == 0) ? item.width : item.length;

            // 在轮廓内搜索
            for (double x = minX + currentLen / 2; x <= maxX - currentLen / 2; x += step) {
                for (double y = minY + currentWid / 2; y <= maxY - currentWid / 2; y += step) {
                    Point center(x, y);

                    // 快速过滤：检查中心点是否在轮廓内
                    if (!pointInPolygon(center)) {
                        continue;
                    }

                    Rectangle candidate(center, item.length, item.width, rot * 90.0);
                    
                    if (validatePlacement(candidate, &item)) {
                        item.placement = candidate;
                        item.placed = true;
                        placed.push_back(candidate);
                    
                        std::cout << "  放置成功 (内部): " << item.name
                            << " 在 " << center
                            << " 旋转=" << (rot * 90) << "°\n";
                        return true;
                    }
                }
            }
        }

        return false;
    }

    // 角落优先策略
    bool tryPlaceInCorner(Item& item) {
        // 找到所有内角点
        std::vector<Point> corners;
        int n = contour.size();

        for (int i = 0; i < n; i++) {
            Point p = contour[i];
            Point prev = contour[(i - 1 + n) % n];
            Point next = contour[(i + 1) % n];

            // 计算两条边的方向
            Point dir1 = p - prev;
            Point dir2 = next - p;

            // 归一化
            double len1 = std::sqrt(dir1.x * dir1.x + dir1.y * dir1.y);
            double len2 = std::sqrt(dir2.x * dir2.x + dir2.y * dir2.y);

            if (len1 > EPSILON && len2 > EPSILON) {
                dir1.x /= len1;
                dir1.y /= len1;
                dir2.x /= len2;
                dir2.y /= len2;

                // 计算夹角
                double dot = dir1.dot(dir2);
                double angle = std::acos(std::max(-1.0, std::min(1.0, dot))) * 180.0 / PI;

                // 如果是内角（小于180度）
                if (angle < 170) {  // 接近直角或锐角
                    corners.push_back(p);
                }
            }
        }

        // 尝试在角落放置
        for (const auto& corner : corners) {
            for (int rot = 0; rot <= 1; rot++) {
                double currentLen = (rot == 0) ? item.length : item.width;
                double currentWid = (rot == 0) ? item.width : item.length;

                // 尝试四种对齐方式
                std::vector<Point> offsets = {
                    Point(currentLen / 2, currentWid / 2),
                    Point(-currentLen / 2, currentWid / 2),
                    Point(currentLen / 2, -currentWid / 2),
                    Point(-currentLen / 2, -currentWid / 2)
                };

                for (const auto& offset : offsets) {
                    Point center = corner + offset;
                    Rectangle candidate(center, item.length, item.width, rot * 90.0);
                    
                    if (validatePlacement(candidate, &item)) {
                        item.placement = candidate;
                        item.placed = true;
                        placed.push_back(candidate);
                    
                        std::cout << "  放置成功 (角落): " << item.name
                            << " 在 " << center << "\n";
                        return true;
                    }
                }
            }
        }

        return false;
    }

    // 优先尝试与已放置物品的边紧贴放置
    bool tryPlaceAdjacentToPlaced(Item& item) {
        if (placed.empty()) return false;

        // 遍历已放置的每个矩形
        for (const auto& placedRect : placed) {
            auto edges = placedRect.getEdges();
            Point placedCenter = placedRect.center;

            for (const auto& edge : edges) {
                Point p1 = edge.p1;
                Point p2 = edge.p2;
                Point edgeDir = p2 - p1;
                double len = std::sqrt(edgeDir.x * edgeDir.x + edgeDir.y * edgeDir.y);
                if (len < EPSILON) continue;
                // 单位方向向量
                edgeDir.x /= len; edgeDir.y /= len;

                // 法线（任意方向），后面确定朝外方向
                Point normal(-edgeDir.y, edgeDir.x);
                Point mid((p1.x + p2.x) / 2.0, (p1.y + p2.y) / 2.0);

                // 判断 normal 指向是否为外侧（远离 placedCenter）
                Point toCenter = placedCenter - mid;
                Point outward = (toCenter.dot(normal) > 0) ? Point(-normal.x, -normal.y) : normal;

                // 已放置矩形到边中点的半宽（沿法线方向）
                double placedHalf = std::abs((placedCenter - mid).dot(outward));

                // 尝试两种旋转（哪边沿边）
                for (int rot = 0; rot <= 1; rot++) {
                    double placeLength = (rot == 0) ? item.length : item.width;
                    double placeWidth  = (rot == 0) ? item.width  : item.length;

                    if (len < placeLength - EPSILON) continue;

                    // 沿边滑动的步长（更细以提高贴合）
                    double step = std::max(5.0, std::min(placeLength, placeWidth) / 6.0);
                    for (double offset = 0; offset <= len - placeLength + EPSILON; offset += step) {
                        Point basePos = p1 + edgeDir * (offset + placeLength / 2.0);

                        // 新矩形中心为沿边位置再向外侧偏移 placedHalf + placeWidth/2
                        Point center = basePos + outward * (placedHalf + placeWidth / 2.0);

                        double itemAngle = std::atan2(edgeDir.y, edgeDir.x) * 180.0 / PI;
                        Rectangle candidate(center, item.length, item.width, itemAngle);

                        if (validatePlacement(candidate, &item)) {
                            item.placement = candidate;
                            item.placed = true;
                            placed.push_back(candidate);
                            std::cout << "  放置成功 (紧贴已放置物边): " << item.name
                                << " 在 " << center << " 旋转=" << itemAngle << "°\n";
                            return true;
                        }
                    }
                }
            }
        }

        return false;
    }

public:
    LayoutSolver(const std::vector<Point>& contour_, const Door& door_)
        : contour(contour_), door(door_) {
        // 计算轮廓中心和包围盒
        double sumX = 0, sumY = 0;
        minX = maxX = contour[0].x;
        minY = maxY = contour[0].y;

        for (const auto& p : contour) {
            sumX += p.x;
            sumY += p.y;
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        }
        contourCenter = Point(sumX / contour.size(), sumY / contour.size());

        std::cout << "轮廓信息:\n";
        std::cout << "  点数: " << contour.size() << "\n";
        std::cout << "  包围盒: (" << minX << ", " << minY << ") -> ("
            << maxX << ", " << maxY << ")\n";
        std::cout << "  面积: " << (maxX - minX) * (maxY - minY) << "\n";

        analyzeWalls();
    }

    void addItem(const Item& item) {
        items.push_back(item);
    }

    bool solve() {
        placed.clear();

        std::cout << "\n开始布局，物品数量: " << items.size() << "\n";

        // 按面积降序排序
        std::sort(items.begin(), items.end(),
            [](const Item& a, const Item& b) {
                // 冰箱优先
                if (a.isFridge() && !b.isFridge()) return true;
                if (!a.isFridge() && b.isFridge()) return false;
                return a.getArea() > b.getArea();
            });

        // 统计
        int fridgeCount = 0, shelfCount = 0, overShelfCount = 0, iceMakerCount = 0;
        for (const auto& item : items) {
            if (item.isFridge()) fridgeCount++;
            else if (item.isShelf()) shelfCount++;
            else if (item.isOverShelf()) overShelfCount++;
            else if (item.isIceMaker()) iceMakerCount++;
        }

        std::cout << "物品统计:\n";
        std::cout << "  冰箱: " << fridgeCount << "\n";
        std::cout << "  货架: " << shelfCount << "\n";
        std::cout << "  离地架: " << overShelfCount << "\n";
        std::cout << "  制冰机: " << iceMakerCount << "\n";
        std::cout << "\n开始放置:\n";

        for (auto& item : items) {
            item.placed = false;
            std::cout << "尝试放置 " << item.name
                << " [" << itemTypeToString(item.type)
                << "] 尺寸=" << item.length << "x" << item.width << "\n";

            bool placed = false;

            // 1. 先尝试贴墙
            for (const auto& space : wallSpaces) {
                if (tryPlaceAgainstWall(item, space)) {
                    placed = true;
                    break;
                }
            }

            // 2. 再尝试角落
            if (!placed) {
                placed = tryPlaceInCorner(item);
            }

            // 2.1 尝试紧贴已放置物的边
            if (!placed) {
                placed = tryPlaceAdjacentToPlaced(item);
            }

            // 3. 最后网格搜索
            if (!placed) {
                placed = tryPlaceInGrid(item);
            }

            if (!placed) {
                std::cout << "❌ 无法放置物品: " << item.name << "\n";
                return false;
            }
        }

        std::cout << "\n✅ 所有物品放置成功!\n";
        return true;
    }

    void printSolution() const {
        std::cout << "\n========== 布局解决方案 ==========\n\n";

        double totalArea = 0;
        for (const auto& item : items) {
            std::cout << item.name
                << " [" << itemTypeToString(item.type) << "]\n";
            std::cout << "  中心点: " << item.placement.center << "\n";
            std::cout << "  旋转角度: " << item.placement.angle << "°\n";

            if (item.isFridge()) {
                auto doorEdge = item.placement.getDoorEdge();
                std::cout << "  开门边: " << doorEdge.p1 << " -> "
                    << doorEdge.p2 << "\n";
            }
            std::cout << "\n";

            totalArea += item.getArea();
        }

        std::cout << "总放置面积: " << totalArea << "\n";
        std::cout << "轮廓面积: " << (maxX - minX) * (maxY - minY) << "\n";
        std::cout << "空间利用率: " << (totalArea * 100 / ((maxX - minX) * (maxY - minY))) << "%\n";
        std::cout << "================================\n";
    }

    const std::vector<Rectangle>& getPlacedItems() const {
        return placed;
    }
};

#endif // LAYOUT_HPP