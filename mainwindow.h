#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsScene>
#include <QStack>
#include <QVector>
#include <QPoint>
#include <QTimer>
#include <QMouseEvent>      // 鼠标事件
#include <QGraphicsRectItem> // 绘制矩形项
#include <QPixmap>          // 用于纹理
#include <QBrush>           // 用于填充
// #include <QMap>            // 替换为 std::map，需要 QPointLess 配合
#include <QMessageBox>      // 消息框
#include <QFileDialog>      // 文件对话框
#include <QTextStream>      // 文本流
#include <QPen>             // 画笔
#include <QRandomGenerator> // 随机数生成器

// 引入标准库，用于 A* 算法中的 std::priority_queue 和 std::map


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

#define MAX_ROWS 100
#define MAX_COLS 100
#define cellSize 20// 定义单元格大小

// 自定义 QPoint 比较器
// 这是一个仿函数，用于 std::map 中 QPoint 键的排序
// std::map 需要键是可排序的，QPoint 默认没有 operator<，所以需要自定义
struct QPointLess {
    bool operator()(const QPoint &p1, const QPoint &p2) const {
        if (p1.x() != p2.x()) {
            return p1.x() < p2.x();
        }
        return p1.y() < p2.y();
    }
};

// Node 结构体，用于 A* 算法
// 定义在头文件中，因为 A* 算法需要在 MainWindow 中使用
struct Node {
    QPoint pos;          // 节点位置
    int g, h, f;         // g: 从起点到当前点的代价，h: 从当前点到终点的估计代价，f: g + h
    Node* parent;        // 父节点指针，用于重构路径

    // 构造函数
    Node(QPoint p, int g_, int h_, Node* par = nullptr)
        : pos(p), g(g_), h(h_), f(g_ + h_), parent(par) {}
};

// CompareNode 结构体，用于 A* 优先队列
// 定义了Node指针的比较规则，使得优先队列按f值从小到大排序
struct CompareNode {
    bool operator()(const Node* a, const Node* b) const {
        return a->f > b->f; // 返回true表示a的优先级低于b（f值更大），即小f值优先级高
    }
};


class MainWindow : public QMainWindow
{
    Q_OBJECT // 宏，必须存在于任何使用Qt元对象系统（信号和槽）的类中

public:
    // 定义编辑模式枚举，用于鼠标点击设置起点/终点
    enum class EditMode { None, SetStart, SetEnd };

    // 构造函数和析构函数
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // UI按钮点击事件槽函数
    void on_btnGenerate_clicked();
    void on_btnSetStart_clicked();
    void on_btnSetEnd_clicked();
    void on_btnClearPath_clicked();
    void on_btnFindPath_clicked();
    void on_btnNextPath_clicked();
    void on_btnShortest_clicked();
    void on_btnLoad_clicked();

    // 路径动画计时器槽函数
    void onAnimationStep();

protected:
    // 重写鼠标按下事件，用于设置起点/终点
    void mousePressEvent(QMouseEvent *event) override;

private:
    Ui::MainWindow *ui; // UI界面指针
    QGraphicsScene *scene; // 图形场景，用于绘制迷宫

    QPixmap wallTexture; // 墙体纹理图片
    QBrush wallBrush; // 墙体画刷，可用于纹理或纯色填充

    // 迷宫尺寸
    int rows, cols;
    // 路径索引和动画索引
    int pathIndex = -1; // 当前显示的是allPaths中的哪条路径
    int animationIndex = 0; // 动画当前进行到animatedPath的哪一步

    QPoint startPoint; // 迷宫起点
    QPoint endPoint; // 迷宫终点

    EditMode currentEditMode; // 当前的编辑模式

    int maze[MAX_ROWS][MAX_COLS] = {0}; // 迷宫数据：0-通路，1-墙壁，2-Prim算法中的前沿点（临时状态）
    QVector<QPoint> blockedPoints; // 暂时未使用的阻塞点列表，可用于将来扩展功能

    QStack<QPoint> currentPath; // 当前绘制的路径（用于动画）
    QVector<QVector<QPoint>> allPaths; // 存储找到的所有路径

    // 动画相关
    QTimer *animationTimer; // 动画计时器
    QVector<QPoint> animatedPath; // 正在进行动画的路径

    // 迷宫生成和绘制函数
    void generateMaze(int r, int c);
    void drawMaze();
    void drawPath(const QStack<QPoint> &path); // 绘制给定路径

    // 寻路算法辅助函数
    int heuristic(const QPoint &a, const QPoint &b); // A*启发函数
    bool aStar(QStack<QPoint> &path, const QVector<QPoint> &blocked = {}); // A*寻路算法
    // DFS查找所有路径：注意这里参数是引用，用于收集路径
    bool findAllPaths(QPoint start, QVector<QPoint> &currentVisitedPath, QVector<QVector<QPoint>> &paths);

    // 文件操作函数
    bool loadMazeFromFile(const QString &filePath);

    // 路径动画启动函数
    void startPathAnimation(const QVector<QPoint> &path);
};

#endif // MAINWINDOW_H
