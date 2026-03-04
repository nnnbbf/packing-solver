#ifndef GEOMETRY_HPP
#define GEOMETRY_HPP

#include <cmath>
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <limits>
#include <memory>

const double EPSILON = 1e-3;  // 放宽精度，适应大坐标
const double PI = 3.14159265358979323846;

struct Point {
    double x, y;

    Point(double x_ = 0, double y_ = 0) : x(x_), y(y_) {}

    bool operator==(const Point& other) const {
        return std::abs(x - other.x) < EPSILON &&
            std::abs(y - other.y) < EPSILON;
    }

    Point operator+(const Point& other) const {
        return Point(x + other.x, y + other.y);
    }

    Point operator-(const Point& other) const {
        return Point(x - other.x, y - other.y);
    }

    Point operator*(double scalar) const {
        return Point(x * scalar, y * scalar);
    }

    double distanceTo(const Point& other) const {
        double dx = x - other.x;
        double dy = y - other.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    double dot(const Point& other) const {
        return x * other.x + y * other.y;
    }

    double cross(const Point& other) const {
        return x * other.y - y * other.x;
    }

    friend std::ostream& operator<<(std::ostream& os, const Point& p) {
        os << "(" << p.x << ", " << p.y << ")";
        return os;
    }
};

struct Line {
    Point p1, p2;

    Line() : p1(0, 0), p2(0, 0) {}
    Line(const Point& p1_, const Point& p2_) : p1(p1_), p2(p2_) {}

    double length() const {
        return p1.distanceTo(p2);
    }

    bool isHorizontal() const {
        return std::abs(p1.y - p2.y) < EPSILON;
    }

    bool isVertical() const {
        return std::abs(p1.x - p2.x) < EPSILON;
    }

    double getAngle() const {
        double dx = p2.x - p1.x;
        double dy = p2.y - p1.y;
        return std::atan2(dy, dx) * 180.0 / PI;
    }

    Point getDirection() const {
        return Point(p2.x - p1.x, p2.y - p1.y);
    }

    // 获取边的法向量（指向轮廓外部）
    Point getOutwardNormal() const {
        Point dir = getDirection();
        return Point(dir.y, -dir.x);  // 顺时针旋转90度得到向外法线
    }

    // 获取边的法向量（指向轮廓内部）
    Point getInwardNormal(const Point& center) const {
        Point outward = getOutwardNormal();
        double len = std::sqrt(outward.x * outward.x + outward.y * outward.y);
        if (len > EPSILON) {
            outward.x /= len;
            outward.y /= len;
        }

        Point mid((p1.x + p2.x) / 2, (p1.y + p2.y) / 2);
        Point toCenter = center - mid;

        // 如果向外法线指向中心，说明实际是向内法线
        if (outward.dot(toCenter) > 0) {
            return outward;
        }
        else {
            return Point(-outward.x, -outward.y);
        }
    }
};

struct Rectangle {
    Point center;
    double length, width;
    double angle;  // 旋转角度（度）

    Rectangle() : center(0, 0), length(0), width(0), angle(0) {}
    Rectangle(const Point& c, double l, double w, double a = 0)
        : center(c), length(l), width(w), angle(a) {
    }

    // 获取四个顶点（考虑旋转）
    std::vector<Point> getVertices() const {
        std::vector<Point> vertices(4);
        double rad = angle * PI / 180.0;
        double cos_a = std::cos(rad);
        double sin_a = std::sin(rad);

        double dx = length / 2;
        double dy = width / 2;

        std::vector<Point> offsets = {
            Point(-dx, -dy), Point(dx, -dy),
            Point(dx, dy), Point(-dx, dy)
        };

        for (int i = 0; i < 4; i++) {
            Point rotated(
                offsets[i].x * cos_a - offsets[i].y * sin_a,
                offsets[i].x * sin_a + offsets[i].y * cos_a
            );
            vertices[i] = center + rotated;
        }

        return vertices;
    }

    // 获取四条边
    std::vector<Line> getEdges() const {
        auto vertices = getVertices();
        std::vector<Line> edges(4);
        for (int i = 0; i < 4; i++) {
            edges[i] = Line(vertices[i], vertices[(i + 1) % 4]);
        }
        return edges;
    }

    // 获取包围盒
    void getBoundingBox(double& minX, double& maxX, double& minY, double& maxY) const {
        auto vertices = getVertices();
        minX = maxX = vertices[0].x;
        minY = maxY = vertices[0].y;
        for (const auto& v : vertices) {
            minX = std::min(minX, v.x);
            maxX = std::max(maxX, v.x);
            minY = std::min(minY, v.y);
            maxY = std::max(maxY, v.y);
        }
    }

    // 检查点是否在矩形内（包括边界）
    bool contains(const Point& p) const {
        auto vertices = getVertices();
        Point local = p - center;
        double rad = -angle * PI / 180.0;
        double cos_a = std::cos(rad);
        double sin_a = std::sin(rad);

        Point rotated(
            local.x * cos_a - local.y * sin_a,
            local.x * sin_a + local.y * cos_a
        );

        return std::abs(rotated.x) <= length / 2 + EPSILON &&
            std::abs(rotated.y) <= width / 2 + EPSILON;
    }

    // 获取冰箱的开门边（假设长边为开门边）
    Line getDoorEdge() const {
        auto vertices = getVertices();
        if (length >= width) {
            // 长边是开门边
            return Line(vertices[0], vertices[1]);
        }
        else {
            // 短边是开门边
            return Line(vertices[1], vertices[2]);
        }
    }
};

// 物品类型
enum class ItemType {
    FRIDGE,      // 冰箱
    SHELF,       // 货架
    OVER_SHELF,  // 离地架
    ICE_MAKER    // 制冰机
};

// 物品类型转字符串
inline std::string itemTypeToString(ItemType type) {
    switch (type) {
    case ItemType::FRIDGE: return "冰箱";
    case ItemType::SHELF: return "货架";
    case ItemType::OVER_SHELF: return "离地架";
    case ItemType::ICE_MAKER: return "制冰机";
    default: return "未知";
    }
}

struct Item {
    std::string name;
    ItemType type;
    double length, width;
    Rectangle placement;  // 放置位置
    bool placed;

    Item(const std::string& n, ItemType t, double l, double w)
        : name(n), type(t), length(l), width(w), placed(false) {
    }

    bool isFridge() const { return type == ItemType::FRIDGE; }
    bool isShelf() const { return type == ItemType::SHELF; }
    bool isOverShelf() const { return type == ItemType::OVER_SHELF; }
    bool isIceMaker() const { return type == ItemType::ICE_MAKER; }

    // 获取当前方向的实际尺寸
    std::pair<double, double> getCurrentDimensions() const {
        if (!placed) return { length, width };
        if (std::abs(placement.angle - 90) < EPSILON ||
            std::abs(placement.angle - 270) < EPSILON) {
            return { width, length };  // 交换长宽
        }
        return { length, width };
    }

    double getArea() const {
        return length * width;
    }
};

#endif // GEOMETRY_HPP