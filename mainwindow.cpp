#include "mainwindow.h"
#include "ui_mainwindow.h" // 包含UI头文件，用于访问UI元素

// 标准库头文件 (已在 mainwindow.h 中引入，但为了清晰性可再次引入)
#include <queue>       // std::priority_queue
#include <vector>      // std::vector
#include <algorithm>   // std::sort, std::abs
#include <cmath>       // std::abs (用于 int/double 等)
#include <cstdlib>     // abs
#include <map>         // std::map

// MainWindow 类的构造函数
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , rows(21) // 默认行数为21
    , cols(21) // 默认列数为21
    , startPoint(1, 1)
    , endPoint(19, 19)
    , currentEditMode(EditMode::None)
{
    ui->setupUi(this);

    // 创建图形场景并设置合适的场景矩形
    scene = new QGraphicsScene(this);
    scene->setSceneRect(0, 0, cols *  cellSize, rows *  cellSize); // 使用CELL_SIZE

    // 设置图形视图
    ui->graphicsView->setScene(scene);
    ui->graphicsView->setRenderHint(QPainter::Antialiasing);

    // 确保视图没有缩放和偏移
    ui->graphicsView->resetTransform();
    ui->graphicsView->setAlignment(Qt::AlignTop | Qt::AlignLeft); // 左上角对齐

    // 初始化墙体纹理
    wallBrush = QBrush(Qt::black); // 默认黑色画刷

    // 尝试加载纹理
    if (!wallTexture.load(":/textures/wall.png")) {
        QMessageBox::warning(this, "警告",
                             "无法加载墙体纹理wall.png，将使用黑色填充。请确保图片存在且已添加到resources.qrc中。");
    } else {
        // 确保纹理大小与CELL_SIZE匹配
        wallBrush = QBrush(wallTexture.scaled( cellSize,  cellSize,
                                              Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    }

    // 连接信号槽
    connect(ui->btnGenerate, &QPushButton::clicked, this, &MainWindow::on_btnGenerate_clicked);
    connect(ui->btnSetStart, &QPushButton::clicked, this, &MainWindow::on_btnSetStart_clicked);
    connect(ui->btnSetEnd, &QPushButton::clicked, this, &MainWindow::on_btnSetEnd_clicked);
    connect(ui->btnClearPath, &QPushButton::clicked, this, &MainWindow::on_btnClearPath_clicked);
    connect(ui->btnFindPath, &QPushButton::clicked, this, &MainWindow::on_btnFindPath_clicked);
    connect(ui->btnShortest, &QPushButton::clicked, this, &MainWindow::on_btnShortest_clicked);
    connect(ui->btnLoad, &QPushButton::clicked, this, &MainWindow::on_btnLoad_clicked);
    connect(ui->btnNextPath, &QPushButton::clicked, this, &MainWindow::on_btnNextPath_clicked);

    // 初始化动画计时器
    animationTimer = new QTimer(this);
    connect(animationTimer, &QTimer::timeout, this, &MainWindow::onAnimationStep);

    // 初始生成并绘制迷宫
    generateMaze(rows, cols);
    drawMaze();
}

// MainWindow 类的析构函数
MainWindow::~MainWindow()
{
    delete ui; // 释放UI界面内存
    // Note: scene管理的QGraphicsItem会在scene析构时自动释放
    // Node对象在aStar函数中已经清理，所以这里通常不需要额外清理
}

// 生成迷宫函数（使用Prim算法）
void MainWindow::generateMaze(int r, int c) {
    // 确保行数和列数为奇数，以适应Prim算法基于网格的生成方式
    if (r % 2 == 0) r--;
    if (c % 2 == 0) c--;

    // 确保最小迷宫尺寸为3x3
    if (r < 3) r = 3;
    if (c < 3) c = 3;

    rows = r;
    cols = c;

    // 初始化所有单元格为墙壁 (1)
    for (int i = 0; i < rows; ++i)
        for (int j = 0; j < cols; ++j)
            maze[i][j] = 1;

    // Prim 算法生成迷宫
    QVector<QPoint> frontier; // 边界点（待处理的墙壁）列表

    // 辅助lambda函数：将指定点添加到frontier列表
    auto addFrontier = [&](int y, int x) {
        // 检查边界并确保是墙壁 (1)
        if (x >= 0 && x < cols && y >= 0 && y < rows && maze[y][x] == 1) {
            frontier.append(QPoint(x, y));
            maze[y][x] = 2; // 标记为“待处理前沿”，避免重复添加
        }
    };

    // 随机选择一个起始点（必须是奇数坐标，以便Prim算法在墙壁网格上工作）
    // 确保起点在迷宫范围内，并尝试找到一个可行的奇数坐标作为起点
    startPoint = QPoint(1, 1);
    if (startPoint.y() >= rows || startPoint.x() >= cols || startPoint.y() % 2 == 0 || startPoint.x() % 2 == 0) {
        // 如果默认起点不合法，尝试在迷宫内找到一个合法的奇数坐标
        startPoint = QPoint((cols - 1) / 2 * 2 + 1, (rows - 1) / 2 * 2 + 1); // 居中奇数点
        if (startPoint.x() >= cols) startPoint.setX(cols - (cols % 2 == 0 ? 3 : 2));
        if (startPoint.y() >= rows) startPoint.setY(rows - (rows % 2 == 0 ? 3 : 2));
        if (startPoint.x() < 1) startPoint.setX(1);
        if (startPoint.y() < 1) startPoint.setY(1);
    }
    maze[startPoint.y()][startPoint.x()] = 0; // 将起始点设为通路

    // 添加起始点周围的初始前沿单元格 (距离为2的墙体)
    addFrontier(startPoint.y(), startPoint.x() + 2);
    addFrontier(startPoint.y(), startPoint.x() - 2);
    addFrontier(startPoint.y() + 2, startPoint.x());
    addFrontier(startPoint.y() - 2, startPoint.x());

    // 主循环：当边界点列表不为空时
    while (!frontier.isEmpty()) {
        // 随机选择一个边界点
        int idx = QRandomGenerator::global()->bounded(frontier.size());
        QPoint f = frontier[idx];
        frontier.removeAt(idx); // 从列表中移除

        int x = f.x();
        int y = f.y();

        // 查找与当前前沿单元格相邻的已挖空路径单元格 (距离为2的通路)
        QVector<QPoint> neighbors;
        for (QPoint d : {QPoint(0, 2), QPoint(0, -2), QPoint(2, 0), QPoint(-2, 0)}) { // 上下左右四个方向
            int nx = x + d.x();
            int ny = y + d.y();
            if (nx >= 0 && nx < cols && ny >= 0 && ny < rows && maze[ny][nx] == 0) {
                neighbors.append(QPoint(nx, ny));
            }
        }

        if (!neighbors.isEmpty()) {
            // 随机选择一个已挖空路径邻居
            QPoint n = neighbors[QRandomGenerator::global()->bounded(neighbors.size())];
            int wallX = (x + n.x()) / 2; // 计算前沿单元格和已挖空邻居之间的墙壁
            int wallY = (y + n.y()) / 2;

            maze[y][x] = 0; // 挖空当前前沿单元格
            maze[wallY][wallX] = 0; // 挖空当前和邻居之间的墙壁

            // 将新挖空路径周围的新前沿单元格添加到frontier列表
            addFrontier(y, x + 2);
            addFrontier(y, x - 2);
            addFrontier(y + 2, x);
            addFrontier(y - 2, x);
        }
    }

    // 设置终点
    endPoint = QPoint(cols - 2, rows - 2); // 默认设置为右下角倒数第二个奇数点
    // 确保终点在迷宫范围内且是通路
    if (endPoint.y() >= rows || endPoint.x() >= cols || endPoint.y() < 0 || endPoint.x() < 0 || maze[endPoint.y()][endPoint.x()] == 1) {
        // 如果默认终点不合法或为墙壁，尝试找到一个合法的奇数坐标通路
        bool foundEnd = false;
        for (int i = rows - 1; i >= 0; --i) { // 从右下角开始向上遍历
            for (int j = cols - 1; j >= 0; --j) { // 从右下角开始向左遍历
                if (maze[i][j] == 0) { // 找到一个通路
                    endPoint = QPoint(j, i);
                    foundEnd = true;
                    break;
                }
            }
            if (foundEnd) break;
        }
        if (!foundEnd) {
            // 极少发生，除非迷宫完全没有通路
            QMessageBox::warning(this, "警告", "生成的迷宫中没有可用终点，请手动设置！");
            // 尝试强制将最远点设为通路，避免死循环
            // 再次检查endPoint是否在有效范围内
            if (endPoint.y() < rows && endPoint.x() < cols && endPoint.y() >= 0 && endPoint.x() >= 0)
                maze[endPoint.y()][endPoint.x()] = 0;
            else // 如果endPoint本身不在有效范围，则尝试一个默认的安全值
                endPoint = QPoint(cols - (cols % 2 == 0 ? 3 : 2), rows - (rows % 2 == 0 ? 3 : 2));
            if (endPoint.x() < 1) endPoint.setX(1);
            if (endPoint.y() < 1) endPoint.setY(1);
            if (endPoint.x() >= cols) endPoint.setX(cols - 2);
            if (endPoint.y() >= rows) endPoint.setY(rows - 2);
            maze[endPoint.y()][endPoint.x()] = 0; // 确保是通路
        }
    }
    maze[endPoint.y()][endPoint.x()] = 0; // 确保终点是通路

    // 清空之前的路径和动画状态
    blockedPoints.clear();
    currentPath.clear();
    allPaths.clear();
    pathIndex = -1;
    animationIndex = 0;
}

// 绘制迷宫函数
void MainWindow::drawMaze() {
    scene->clear(); // 清空场景中所有元素

    // 绘制迷宫墙体（实心方块）
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            if (maze[y][x] == 1) { // 如果是墙壁单元格
                QGraphicsRectItem *wallRect = new QGraphicsRectItem(x * cellSize, y * cellSize, cellSize, cellSize);
                wallRect->setBrush(wallBrush); // 使用纹理或黑色填充
                wallRect->setPen(Qt::NoPen);    // 不绘制边框，使其看起来更像实心
                scene->addItem(wallRect); // 添加到场景
            }
        }
    }

    // 绘制四周边界线条（外层边框，粗一点）
    int widthPx = cols * cellSize;
    int heightPx = rows * cellSize;
    int borderWidth = 3;
    QPen borderPen(Qt::black);
    borderPen.setWidth(borderWidth);

    scene->addLine(0, 0, widthPx, 0, borderPen);         // 上边界
    scene->addLine(0, heightPx, widthPx, heightPx, borderPen); // 下边界
    scene->addLine(0, 0, 0, heightPx, borderPen);         // 左边界
    scene->addLine(widthPx, 0, widthPx, heightPx, borderPen);     // 右边界

    // 绘制起点：红色圆点
    QPoint startCenter(startPoint.x() * cellSize + cellSize / 2,
                       startPoint.y() * cellSize + cellSize / 2);
    scene->addEllipse(startCenter.x() - 5, startCenter.y() - 5, 10, 10, // 圆心(x,y)，半径5
                      QPen(Qt::red), QBrush(Qt::red));

    // 绘制终点：绿色圆点
    QPoint endCenter(endPoint.x() * cellSize + cellSize / 2,
                     endPoint.y() * cellSize + cellSize / 2);
    scene->addEllipse(endCenter.x() - 5, endCenter.y() - 5, 10, 10,
                      QPen(Qt::blue), QBrush(Qt::blue));
}

// 绘制路径函数
void MainWindow::drawPath(const QStack<QPoint> &path) {
    drawMaze(); // 首先绘制迷宫墙体和起终点

    // 路径：高亮绿色，半透明，更粗的线条
    QPen pathPen(QColor(0, 255, 0, 180)); // 绿色，alpha 180 (稍微半透明)
    pathPen.setWidth(5); // 设置线宽为5像素

    // 遍历路径点，绘制连接线
    for (int i = 1; i < path.size(); ++i) {
        // 计算当前点和上一个点的中心坐标
        QPointF p1(path[i - 1].x() * cellSize + cellSize / 2.0,
                   path[i - 1].y() * cellSize + cellSize / 2.0);
        QPointF p2(path[i].x() * cellSize + cellSize / 2.0,
                   path[i].y() * cellSize + cellSize / 2.0);
        scene->addLine(QLineF(p1, p2), pathPen); // 绘制线段
    }
}

// 鼠标按下事件处理函数
// mainwindow.cpp
void MainWindow::mousePressEvent(QMouseEvent *event) {
    // 只有在左键点击且处于设置起点/终点模式时才处理
    if (event->button() != Qt::LeftButton ||
        (currentEditMode != EditMode::SetStart && currentEditMode != EditMode::SetEnd)) {
        QMainWindow::mousePressEvent(event); // 调用基类的事件处理，让其他事件正常传递
        return;
    }

    // --- 关键修正区域 ---
    // 检查鼠标事件是否发生在 graphicsView 控件内部
    if (!ui->graphicsView->geometry().contains(event->pos())) {
        // 如果点击不在 graphicsView 范围内，则不处理，并传递给基类
        QMainWindow::mousePressEvent(event);
        return;
    }

    // 将鼠标点击的 MainWindow 坐标转换为 QGraphicsView 的局部坐标
    // 这是一个更安全和推荐的方法，因为 event->pos() 是相对于 MainWindow 的
    QPoint viewLocalPos = ui->graphicsView->mapFromGlobal(event->globalPos());

    // 将 QGraphicsView 局部坐标转换为场景坐标
    QPointF scenePos = ui->graphicsView->mapToScene(viewLocalPos);
    // --- 关键修正区域结束 ---

    // 转换为迷宫网格坐标
    // 确保这里使用的是您定义的 CELL_SIZE (如果我之前建议您将 cellSize 改为 CELL_SIZE)
    int col = static_cast<int>(scenePos.x()) /  cellSize ;
    int row = static_cast<int>(scenePos.y()) /  cellSize ;

    // 严格边界检查
    if (col < 0 || col >= cols || row < 0 || row >= rows) {
        QMessageBox::warning(this, "警告", "点击位置超出迷宫范围！");
        // 不重置编辑模式，以便用户可以再次尝试点击
        return;
    }

    // 检查是否是墙壁
    // maze 是二维 QVector 的话，需要使用 maze[row][col]
    if (maze[row][col] == 1) {
        QMessageBox::warning(this, "警告", "不能将起点或终点设置在墙壁上！请点击通路。");
        // 不重置编辑模式，以便用户可以再次尝试点击
        return;
    }

    // 设置起点或终点
    if (currentEditMode == EditMode::SetStart) {
        // 检查新起点是否与终点重合
        if (QPoint(col, row) == endPoint) {
            QMessageBox::warning(this, "警告", "起点不能与终点相同！");
            return;
        }
        startPoint = QPoint(col, row);
        ui->statusbar->showMessage(QString("起点已设置为 (%1,%2)").arg(col).arg(row), 3000);
    }
    else if (currentEditMode == EditMode::SetEnd) {
        // 检查新终点是否与起点重合
        if (QPoint(col, row) == startPoint) {
            QMessageBox::warning(this, "警告", "终点不能与起点相同！");
            return;
        }
        endPoint = QPoint(col, row);
        ui->statusbar->showMessage(QString("终点已设置为 (%1,%2)").arg(col).arg(row), 3000);
    }

    // 只有在成功设置了有效点后，才重置编辑模式
    drawMaze(); // 重绘迷宫以显示新的起终点
    currentEditMode = EditMode::None; // 重置编辑模式

    // QMainWindow::mousePressEvent(event); // 这里不需再次调用，因为我们已经完全处理了事件
}



// 启发函数（曼哈顿距离）
int MainWindow::heuristic(const QPoint &a, const QPoint &b) {
    return std::abs(a.x() - b.x()) + std::abs(a.y() - b.y()); // 计算两点之间的曼哈顿距离
}

// A* 寻路算法
// A* 寻路算法 (已修正)
bool MainWindow::aStar(QStack<QPoint> &path, const QVector<QPoint> &blocked) {
    std::priority_queue<Node*, std::vector<Node*>, CompareNode> open;
    std::map<QPoint, Node*, QPointLess> allNodes; // 使用 std::map 存储 QPoint 键

    Node* startNode = new Node(startPoint, 0, heuristic(startPoint, endPoint));
    open.push(startNode);
    allNodes[startPoint] = startNode;

    Node* endNode = nullptr;

    while (!open.empty()) {
        Node* current = open.top();
        open.pop();

        // 这是一个优化，如果从优先队列中取出的节点不是到达该点的最优路径，则跳过
        // （因为一个更优的路径已经被处理过了）
        auto it = allNodes.find(current->pos);
        if (it != allNodes.end() && current->f > it->second->f) {
            // 这个current节点是一个被更优路径取代的旧节点，可以直接忽略。
            // 注意：这里不要 delete current，因为它会在最后的统一清理中被释放。
            continue;
        }

        if (current->pos == endPoint) {
            endNode = current;
            break; // 找到终点，退出循环
        }

        // 探索邻居节点
        for (QPoint d : {QPoint(0,1), QPoint(1,0), QPoint(0,-1), QPoint(-1,0)}) { // 四个方向
            QPoint next = current->pos + d;
            int x = next.x(), y = next.y();

            // 边界检查、墙壁检查、阻塞点检查
            if (x < 0 || x >= cols || y < 0 || y >= rows || maze[y][x] == 1 || blocked.contains(next)) {
                continue;
            }

            int tentative_gScore = current->g + 1;

            Node* neighborNode = nullptr;
            auto it2 = allNodes.find(next);
            if (it2 != allNodes.end()) {
                neighborNode = it2->second;
            }

            // 如果是新节点，或者找到了到邻居节点的更短路径
            if (!neighborNode || tentative_gScore < neighborNode->g) {
                if (neighborNode) {
                    // 更新现有节点信息
                    neighborNode->parent = current;
                    neighborNode->g = tentative_gScore;
                    neighborNode->f = neighborNode->g + heuristic(neighborNode->pos, endPoint);
                    open.push(neighborNode); // 将更新后的节点重新放入优先队列
                } else {
                    // 创建新节点
                    Node* newNode = new Node(next, tentative_gScore, heuristic(next, endPoint), current);
                    allNodes[next] = newNode;
                    open.push(newNode);
                }
            }
        }
    }

    // --- 以下是核心修正区域 ---

    bool pathFound = (endNode != nullptr);

    // 步骤 1: 如果找到了路径，先重建它
    if (pathFound) {
        // 重构路径：从终点节点回溯到起点节点
        // 因为我们是把 QPoint (值) 压入栈，所以即使稍后 Node* 被 delete，路径数据也是安全的。
        path.clear();
        QStack<QPoint> reversePath;
        Node* node = endNode;
        while (node) {
            reversePath.push(node->pos); // 将节点位置压入栈
            node = node->parent;         // 移向父节点
        }

        // 将反向路径栈转换为正向路径
        while (!reversePath.isEmpty()) {
            path.push(reversePath.pop());
        }
    }

    // 步骤 2: 无论是否找到路径，最后统一清理所有分配的内存
    for (auto const& [key, val] : allNodes) {
        delete val;
    }
    allNodes.clear(); // 清空 map

    // 步骤 3: 返回结果
    return pathFound;
}


// 查找所有路径函数（使用DFS）
// 注意：对于大型迷宫和多条路径，此函数可能非常耗时且消耗大量内存。
// 限制paths.size()是一个简单的保护措施。
bool MainWindow::findAllPaths(QPoint start, QVector<QPoint> &currentVisitedPath, QVector<QVector<QPoint>> &paths) {
    // 终止条件：如果当前点是终点
    if (start == endPoint) {
        paths.append(currentVisitedPath); // 将当前路径添加到所有路径列表中
        return true;
    }

    // 限制查找的路径数量，防止过度计算/内存消耗
    if (paths.size() > 2000) { // 限制为 2000 条路径
        return false; // 达到限制，不再继续查找
    }

    // 定义四个方向
    QVector<QPoint> directions = {QPoint(0,1), QPoint(1,0), QPoint(0,-1), QPoint(-1,0)};
    for (QPoint d : directions) {
        QPoint next = start + d; // 计算下一个点的位置
        int x = next.x(), y = next.y();

        // 边界检查、墙壁检查、避免当前路径中的循环
        if (x < 0 || x >= cols || y < 0 || y >= rows) continue; // 超出边界
        if (maze[y][x] == 1) continue; // 是墙壁
        if (currentVisitedPath.contains(next)) continue; // 避免在当前路径中形成循环

        currentVisitedPath.append(next); // 将下一个点添加到当前访问路径
        findAllPaths(next, currentVisitedPath, paths); // 递归调用，探索新分支
        currentVisitedPath.removeLast(); // 回溯：移除当前点，以便探索其他分支
    }
    return !paths.isEmpty(); // 返回是否找到至少一条路径
}

// 从文件加载迷宫
bool MainWindow::loadMazeFromFile(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // 文件无法打开
        QMessageBox::warning(this, "错误", "无法打开文件。");
        return false;
    }

    QTextStream in(&file);
    QStringList lines;
    while (!in.atEnd()) {
        lines.append(in.readLine().trimmed()); // 读取每行并去除空白字符
    }
    file.close();

    if (lines.isEmpty()) {
        QMessageBox::warning(this, "错误", "文件为空。");
        return false;
    }

    int r = lines.size(); // 行数
    int c = lines[0].length(); // 列数 (以第一行长度为准)

    // 检查迷宫尺寸是否超出最大限制
    if (r > MAX_ROWS || c > MAX_COLS) {
        QMessageBox::warning(this, "错误", QString("迷宫尺寸过大！最大支持%1x%2。").arg(MAX_ROWS).arg(MAX_COLS));
        return false;
    }

    // 解析文件内容到maze数组
    for (int i = 0; i < r; ++i) {
        if (lines[i].length() != c) {
            QMessageBox::warning(this, "错误", "文件格式不正确：行长度不一致。");
            return false;
        }
        for (int j = 0; j < c; ++j) {
            QChar ch = lines[i][j];
            if (ch == '0')
                maze[i][j] = 0; // '0' 表示通路
            else if (ch == '1')
                maze[i][j] = 1; // '1' 表示墙壁
            else {
                QMessageBox::warning(this, "错误", QString("文件格式不正确：包含非法字符 '%1'。").arg(ch));
                return false;
            }
        }
    }
    rows = r; cols = c; // 更新迷宫的实际行数和列数

    // 默认设置起点和终点
    startPoint = QPoint(0, 0);
    endPoint = QPoint(cols - 1, rows - 1);

    // 确保加载后起点是可走的路径
    if (maze[startPoint.y()][startPoint.x()] == 1) {
        bool foundStart = false;
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j) {
                if (maze[i][j] == 0) { // 找到第一个通路作为起点
                    startPoint = QPoint(j, i);
                    foundStart = true;
                    break;
                }
            }
            if (foundStart) break;
        }
        if (!foundStart) {
            QMessageBox::warning(this, "警告", "加载的迷宫中没有通路！无法设置起点。");
            return false;
        }
    }

    // 确保加载后终点是可走的路径
    if (maze[endPoint.y()][endPoint.x()] == 1) {
        bool foundEnd = false;
        for (int i = rows - 1; i >= 0; --i) { // 从右下角开始找
            for (int j = cols - 1; j >= 0; --j) {
                if (maze[i][j] == 0) { // 找到最后一个通路作为终点
                    endPoint = QPoint(j, i);
                    foundEnd = true;
                    break;
                }
            }
            if (foundEnd) break;
        }
        if (!foundEnd) {
            QMessageBox::warning(this, "警告", "加载的迷宫中没有通路！无法设置终点。");
            return false;
        }
    }

    // 清空之前的路径和动画状态，因为迷宫已改变
    blockedPoints.clear();
    currentPath.clear();
    allPaths.clear();
    pathIndex = -1;
    animationIndex = 0;

    return true; // 迷宫加载成功
}

// 启动路径动画函数
void MainWindow::startPathAnimation(const QVector<QPoint> &path)
{
    if (animationTimer->isActive())
        animationTimer->stop(); // 如果计时器正在运行，先停止

    animatedPath = path; // 设置要动画的路径
    animationIndex = 0; // 重置动画步数
    currentPath.clear(); // 清空当前绘制的路径

    if (!animatedPath.isEmpty()) {
        animationTimer->start(80);  // 启动计时器，每80毫秒触发一次onAnimationStep
    } else {
        drawMaze(); // 如果路径为空，只重绘迷宫，不显示路径
    }
}

// 动画步进函数
void MainWindow::onAnimationStep()
{
    if (animationIndex >= animatedPath.size()) {
        animationTimer->stop(); // 动画播放完毕，停止计时器
        return;
    }

    currentPath.push(animatedPath[animationIndex]); // 将当前步的路径点加入绘制列表
    animationIndex++; // 动画步数加一

    drawPath(currentPath); // 绘制当前动画状态的路径
}

// "生成迷宫"按钮点击槽函数
void MainWindow::on_btnGenerate_clicked()
{
    generateMaze(rows, cols); // 根据当前行、列数生成新迷宫
    drawMaze(); // 绘制新迷宫
    ui->statusbar->showMessage("迷宫已生成。", 3000); // 状态栏提示
}

// "设置起点"按钮点击槽函数
void MainWindow::on_btnSetStart_clicked()
{
    currentEditMode = EditMode::SetStart; // 设置编辑模式为设置起点
    ui->statusbar->showMessage("点击通路设置起点...", 3000); // 状态栏提示
}

// "设置终点"按钮点击槽函数
void MainWindow::on_btnSetEnd_clicked()
{
    currentEditMode = EditMode::SetEnd; // 设置编辑模式为设置终点
    ui->statusbar->showMessage("点击通路设置终点...", 3000); // 状态栏提示
}

// "清除路径"按钮点击槽函数
void MainWindow::on_btnClearPath_clicked()
{
    currentPath.clear(); // 清空当前绘制路径
    allPaths.clear(); // 清空所有找到的路径
    blockedPoints.clear(); // 清除可能的阻塞点（如果未来有此功能）
    pathIndex = -1; // 重置路径索引
    animationTimer->stop(); // 停止任何正在进行的动画
    drawMaze(); // 重新绘制迷宫，清除路径显示
    ui->statusbar->showMessage("路径和动画已清除。", 3000);
}

// "寻找最短路径"按钮点击槽函数 (使用A*算法)
void MainWindow::on_btnFindPath_clicked()
{
    currentPath.clear();
    blockedPoints.clear();
    allPaths.clear(); // 确保清空所有路径，因为这是查找最短路径
    pathIndex = -1;
    animationTimer->stop(); // 停止动画

    // 检查起点和终点是否在迷宫范围内且是通路
    if (startPoint.y() < 0 || startPoint.y() >= rows || startPoint.x() < 0 || startPoint.x() >= cols || maze[startPoint.y()][startPoint.x()] == 1 ||
        endPoint.y() < 0 || endPoint.y() >= rows || endPoint.x() < 0 || endPoint.x() >= cols || maze[endPoint.y()][endPoint.x()] == 1)
    {
        QMessageBox::warning(this, "错误", "起点或终点不在迷宫范围内或为墙壁，无法查找路径。请重新设置。");
        return;
    }

    // 调用A*算法查找最短路径
    if (aStar(currentPath, blockedPoints)) {
        startPathAnimation(currentPath.toVector()); // 找到路径则启动动画
        ui->statusbar->showMessage(QString("找到最短路径，共 %1 步。").arg(currentPath.size()), 5000);
    } else {
        QMessageBox::information(this, "提示", "找不到路径！请检查起终点或迷宫结构。");
        drawMaze(); // 未找到路径，只重绘迷宫
    }
}

// "下一条路径"按钮点击槽函数 (查找所有路径后，逐条显示)
void MainWindow::on_btnNextPath_clicked()
{
    animationTimer->stop(); // 停止当前动画

    // 如果还没有找到所有路径，则先查找
    if (allPaths.isEmpty() || pathIndex == -1) {
        allPaths.clear(); // 确保查找所有路径时是一个干净的状态
        QVector<QPoint> currentVisitedPathForDFS;
        currentVisitedPathForDFS.append(startPoint); // DFS从起点开始

        // 检查起点和终点是否在迷宫范围内且是通路
        if (startPoint.y() < 0 || startPoint.y() >= rows || startPoint.x() < 0 || startPoint.x() >= cols || maze[startPoint.y()][startPoint.x()] == 1 ||
            endPoint.y() < 0 || endPoint.y() >= rows || endPoint.x() < 0 || endPoint.x() >= cols || maze[endPoint.y()][endPoint.x()] == 1)
        {
            QMessageBox::warning(this, "错误", "起点或终点不在迷宫范围内或为墙壁，无法查找路径。请重新设置。");
            return;
        }

        findAllPaths(startPoint, currentVisitedPathForDFS, allPaths); // 调用DFS查找所有路径

        // 按长度对找到的路径进行排序 (最短的在前)
        std::sort(allPaths.begin(), allPaths.end(), [](const QVector<QPoint>& a, const QVector<QPoint>& b) {
            return a.size() < b.size();
        });

        if (allPaths.isEmpty()) {
            QMessageBox::information(this, "提示", "找不到任何路径！请检查起终点或迷宫结构。");
            pathIndex = -1; // 确保索引重置
            return;
        }
        pathIndex = 0; // 设置为第一条路径
    } else {
        // 如果已经查找过所有路径，则显示下一条
        if (pathIndex + 1 >= allPaths.size()) {
            QMessageBox::information(this, "提示", QString("已经是最后一条路径，共找到 %1 条路径。").arg(allPaths.size()));
            return;
        }
        pathIndex++; // 移动到下一条路径
    }

    startPathAnimation(allPaths[pathIndex]); // 启动当前路径的动画

    QString info = QString("当前为第 %1 条路径，共 %2 步。")
                       .arg(pathIndex + 1)
                       .arg(allPaths[pathIndex].size());
    ui->statusbar->showMessage(info, 5000); // 状态栏显示当前路径信息
}

// "显示最短路径"按钮点击槽函数 (直接绘制最短路径，不动画)
void MainWindow::on_btnShortest_clicked() {
    currentPath.clear();
    blockedPoints.clear();
    allPaths.clear();
    pathIndex = -1;
    animationTimer->stop();

    // 检查起点和终点是否在迷宫范围内且是通路
    if (startPoint.y() < 0 || startPoint.y() >= rows || startPoint.x() < 0 || startPoint.x() >= cols || maze[startPoint.y()][startPoint.x()] == 1 ||
        endPoint.y() < 0 || endPoint.y() >= rows || endPoint.x() < 0 || endPoint.x() >= cols || maze[endPoint.y()][endPoint.x()] == 1)
    {
        QMessageBox::warning(this, "错误", "起点或终点不在迷宫范围内或为墙壁，无法查找最短路径。请重新设置。");
        return;
    }

    // 调用A*算法查找最短路径
    if (aStar(currentPath, blockedPoints)) {
        drawPath(currentPath); // 直接绘制最短路径，不进行动画
        ui->statusbar->showMessage(QString("找到最短路径，共 %1 步。").arg(currentPath.size()), 5000);
    } else {
        QMessageBox::information(this, "提示", "找不到最短路径！请检查起终点或迷宫结构。");
        drawMaze(); // 未找到路径，只重绘迷宫
    }
}

// "加载迷宫"按钮点击槽函数
void MainWindow::on_btnLoad_clicked()
{
    // 打开文件对话框，让用户选择迷宫文件
    QString filePath = QFileDialog::getOpenFileName(this, "加载迷宫文件", "", "文本文件 (*.txt);;所有文件 (*.*)");
    if (filePath.isEmpty()) return; // 用户取消选择

    // 尝试从文件加载迷宫
    if (loadMazeFromFile(filePath)) {
        drawMaze(); // 加载成功则绘制迷宫
        ui->statusbar->showMessage("迷宫加载成功！", 3000);
    } else {
        // 加载失败时，loadMazeFromFile会弹出具体错误信息
        // 这里可以给一个通用的失败提示
        QMessageBox::warning(this, "错误", "迷宫加载失败。");
    }
}
