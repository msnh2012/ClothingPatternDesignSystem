#include "myrule.h"
#include <QStack>
#include <QChar>
#include <QTextCodec>
#include <QMessageBox>
#include <QInputDialog>
#include "mypainter.h"

/**
 * @brief 弹出提示
 * @param info 提示信息
 */
void MyRule::info(QString info)
{
    QMessageBox::information(nullptr,"information",info);
}

/**
 * @brief 弹出实体输入框
 * @param type 实体类型
 * @param name 实体名
 * @return 赋值是否成功
 */
bool MyRule::setInput(QString type, QString name)
{
    bool ok = true;
    QPointF point;
    Line line;
    QString v = "";
    if(parentRule == nullptr)
        v = QInputDialog::getText(nullptr,"初始化-输入实体",type+name+" =",
                                  QLineEdit::Normal,"单位：cm（不要保留单位）",&ok);
    else if(entitiesIn.isEmpty()){
        info("找不到输入的"+type+"实体"+name);
        v = QInputDialog::getText(nullptr,"初始化-输入实体",type+name+" =",
                                  QLineEdit::Normal,"单位：cm（不要保留单位）",&ok);
    }else{
        v = entitiesIn.first();
        entitiesIn.removeFirst();
        switch(types.indexOf(type)){
        case 0:
            v = QString::number(parentRule->param(v, &ok));
            if(ok)
                params.insert(name,v);
            break;
        case 1:
            point = parentRule->point(v, &ok);
            if(ok)
                points.insert(name,point);
            break;
        case 2:
            line = parentRule->line(v, &ok);
            if(ok)
                lines.insert(name,line);
            break;
        case 3:
        case -1:
            ok = false;
        }
        return ok;
    }
    v = pretreat(v);
    if(ok){
        switch(types.indexOf(type)){
        case 0:
            v = QString::number(param(v, &ok));
            if(ok)
                params.insert(name,v);
            break;
        case 1:
            point = this->point(v, &ok);
            if(ok)
                points.insert(name,point);
            break;
        case 2:
            line = this->line(v, &ok);
            if(ok)
                lines.insert(name,line);
            break;
        case 3:
        case -1:
            ok = false;
        }
    }
    return ok;
}

/**
 * @brief 找到同文件下的ruleName对应的路径
 * @param ruleName 规则文件名（不含后缀）
 * @return 失败返回""
 */
QString MyRule::findRulePath(QString ruleName)
{
    QString s = file.left(file.lastIndexOf('/')+1);
    return s + ruleName + ".txt";
}

/**
 * @brief p1是否在p2的左侧（x轴向右）
 * @param p1
 * @param p2
 * @return
 */
bool MyRule::left(QPointF p1, QPointF p2)
{
    qreal dx = p1.x()-p2.x();
    if(dx <= -0.0001) return true;
    else return false;
}

/**
 * @brief p1是否在p2的右侧（x轴向右）
 * @param p1
 * @param p2
 * @return
 */
bool MyRule::right(QPointF p1, QPointF p2)
{
    qreal dx = p1.x()-p2.x();
    if(dx >= 0.0001) return true;
    else return false;
}

/**
 * @brief p1是否在p2的下方（y轴向下）
 * @param p1
 * @param p2
 * @return
 */
bool MyRule::down(QPointF p1, QPointF p2)
{
    qreal dy = p1.y()-p2.y();
    if(dy >= 0.0001) return true;
    else return false;
}

/**
 * @brief p1是否在p2的上方（y轴向下）
 * @param p1
 * @param p2
 * @return
 */
bool MyRule::up(QPointF p1, QPointF p2)
{
    qreal dy = p1.y()-p2.y();
    if(dy <= -0.0001) return true;
    else return false;
}

/**
 * @brief 求制板公式的值（暂不支持负数）
 * @param s 制板公式的中缀表达式
 * @param ok 失败时设置为false
 * @return
 */
qreal MyRule::calculate(QString s, bool *ok)
{
    // 求后缀表达式
    QString postOrder;
    QStack<QChar> symbol;
    QStringList operators = {"+" ,"-", "*", "/"};
    QMap<QChar,int> priority = {
        {'(',0},
        {'+',1},
        {'-',1},
        {'*',2},
        {'/',2},
    };
    for(int i = 0;i < s.length(); ++i) {
        if(s[i] == ' ') {
            continue ;
        }
        // case: 操作数
        else if(s[i].isDigit()) {
            postOrder += s[i];
            while(i+1 < s.length() && (s[i+1].isDigit() || s[i+1] == '.'))
                postOrder += s[++i];
        }
        // case: 左括号（开圆括号）
        else if(s[i] == '(')
            symbol.push(s[i]);
        // case: 右括号（闭圆括号）
        else if(s[i] == ')') {
            while(!symbol.isEmpty() && symbol.top() != '(') {
                postOrder += symbol.pop();
                postOrder += " ";
            }
            if(!symbol.isEmpty()) symbol.pop(); // 去掉'('
        }
        // case: 运算符
        else if(s[i] == '+' || s[i] == '-' || s[i] == '*' || s[i] == '/'){
            if(symbol.isEmpty() || priority[s[i]] > priority[symbol.top()])
                symbol.push(s[i]);
            else{
                while(!symbol.isEmpty() && priority[s[i]] <= priority[symbol.top()]) {
                    postOrder += symbol.pop();
                    postOrder += " ";
                }
                symbol.push(s[i]);
            }
        }
        // case: 无法识别的字符
        else{
            if(ok != nullptr) *ok = false;
            return 0;
        }
        if(!postOrder.isEmpty() && postOrder.back() != ' ') {
            postOrder += " ";
        }
    }
    while(!symbol.empty()) {
        postOrder += symbol.top();
        symbol.pop();
        postOrder += " ";
    }
    // 利用后缀表达式来求值
    QStringList sl = postOrder.split(' ',QString::SkipEmptyParts);
    QStack<qreal> st;
    qreal num = 0;
    int i = 0;
    while(i < sl.length()) {
        bool isReal = true;
        num = sl[i].toDouble(&isReal);
        if(isReal) {
            st.push(num);
            i++;
        } else {
            qreal num1 = st.pop();
            qreal num2 = st.pop();
            switch(operators.indexOf(sl[i])) {
            case 0 : st.push(num2 + num1); break;
            case 1 : st.push(num2 - num1); break;
            case 2 : st.push(num2 * num1); break;
            case 3 : st.push(num2 / num1); break;
            }
            i++;
        }
    }
    return st.top();
}

/**
 * @brief 取直线的端点
 * @param l 直线
 * @param s 上下左右（注意竖直线的左=上、右=下，水平线的上=左、下=右）
 * @param ok s非法 或 直线两端点重合 时赋值为false
 * @return
 */
QPointF MyRule::endPoint(Line l, QString s, bool* ok)
{
    return endPoint(l.p1, l.p2, s, ok);
}

/**
 * @brief 取两点中的一点
 * @param p1
 * @param p2
 * @param s 上下左右（注意竖直两点的左=上、右=下，水平两点的上=左、下=右）
 * @param ok s非法 或 两点重合 时赋值为false
 * @return
 */
QPointF MyRule::endPoint(QPointF p1, QPointF p2, QString s, bool* ok)
{
    s = s.simplified();
    if(s == "左端点" || s == "左"){
        if(left(p1,p2)) return p1;
        else if(right(p1,p2)) return p2;
        else if(up(p1,p2)) return p1;
        else if(down(p1,p2)) return p2;
    }else if(s == "右端点" || s == "右"){
        if(left(p1,p2)) return p2;
        else if(right(p1,p2)) return p1;
        else if(up(p1,p2)) return p2;
        else if(down(p1,p2)) return p1;
    }
    if(s == "上端点" || s == "上"){
        if(up(p1,p2)) return p1;
        else if(down(p1,p2)) return p2;
        else if(left(p1,p2)) return p1;
        else if(right(p1,p2)) return p2;
    }else if(s == "下端点" || s == "下"){
        if(up(p1,p2)) return p2;
        else if(down(p1,p2)) return p1;
        else if(left(p1,p2)) return p2;
        else if(right(p1,p2)) return p1;
    }
    if(ok != nullptr) *ok =false;
    return p1;
}

/**
 * @brief 构造函数
 * @param f 规则文件的路径
 */
MyRule::MyRule(QString f)
{
    types << "参数" << "点" << "直线" << "路径" << "曲线";
    pFuncs << "求偏移" << "方向向量" << "求垂足" << "等分点" << "求交点";
    file = f;
    entityOut = "";
    parentRule = nullptr;
}

MyRule::~MyRule(){}

/**
 * @brief 基本约束方法的前5种（求点）
 * @param func 形如“方法名(实体参数表)”的字符串
 * @param idFunc 方法的索引，默认为-1
 * @param ok 必要时赋值为false
 * @return
 */
QPointF MyRule::pFunc(QString func, int idFunc, bool* ok)
{
    QPointF p = QPointF(0,0);
    if(idFunc == -1){
        for(idFunc=0;idFunc<pFuncs.length();idFunc++)
            if(func.contains(pFuncs[idFunc]))
                break;
        if(idFunc == pFuncs.length()){
            if(ok != nullptr) *ok = false;
            return p;
        }
    }
    int id = func.indexOf('(');
    func[id] = ',';
    id = func.lastIndexOf(')');
    func[id] = ' ';
    QStringList en = func.split(",",QString::SkipEmptyParts);
    for(int i=1;i<en.length();i++)
        en[i] = en[i].simplified();
    switch (idFunc) {
    case 0:
        return offset(point(en[1],ok),param(en[2],ok),point(en[3],ok));
    case 1:
        return direction(point(en[1],ok),point(en[2],ok));
    case 2:
        return foot(point(en[1],ok),line(en[2],ok));
    case 3:
        return divide(point(en[1],ok),point(en[2],ok),param(en[3],ok));
    case 4:
        if(getTypeOf(en[1]) == types[2]){
            if(getTypeOf(en[2]) == types[2])
                return cross(line(en[1],ok),line(en[2],ok));
            else if(getTypeOf(en[2]) == types[4])
                return cross(line(en[1],ok),curve(en[2],ok));
        }else if(getTypeOf(en[1]) == types[4]){
            if(getTypeOf(en[2]) == types[2])
                return cross(line(en[2],ok),curve(en[1],ok));
            else if(getTypeOf(en[2]) == types[4])
                return cross(curve(en[1],ok),curve(en[2],ok));
        }
    }
    return p;
}

/**
 * @brief 求偏移点
 * @param p1 参照点
 * @param distance 距离
 * @param direction 方向向量（注意y轴向下）
 * @return
 */
QPointF MyRule::offset(QPointF p1, qreal distance, QPointF direction)
{
    qreal xd = direction.x(), yd = direction.y(),
          ld = sqrt(xd*xd + yd*yd), scale = distance/ld;
    return QPointF(p1.x()+scale*xd, p1.y()+scale*yd);
}

/**
 * @brief 求方向向量
 * @param p1 起点
 * @param p2 终点
 * @param ok 两点重合时赋值为false
 * @return
 */
QPointF MyRule::direction(QPointF p1, QPointF p2, bool *ok)
{
    qreal dx=p2.x()-p1.x(), dy=p2.y()-p1.y();
    if(zero(dx)){
        if(zero(dy)){
            if(ok != nullptr) *ok = false;
            return QPointF(0,0);
        }
        return QPointF(0,dy);
    }
    if(zero(dy))
        return QPointF(dx,0);
    else
        return QPointF(dx,dy);
}



/**
 * @brief 求垂足
 * @param p1 垂线上、直线外的一点
 * @param l1 直线
 * @param ok p1在直线上时赋值为false
 * @return p1在直线上时返回p1
 */
QPointF MyRule::foot(QPointF p1, Line l1, bool *ok)
{
    qreal x1=p1.x(), y1=p1.y(),
          dxl1=l1.p1.x()-l1.p2.x(),
          dyl1=l1.p1.y()-l1.p2.y();
    // case: l1竖直
    if(zero(dxl1))
        return QPointF(l1.p1.x(),y1);
    else{
        qreal k1 = dyl1/dxl1;
        // case: l1倾斜
        if(!zero(k1)){
            qreal k2 = -1/k1, b2 = y1-k2*x1, b1 = l1.p1.y()-l1.p1.x()*k1,
                  x = (b2 - b1)/(k1 - k2), y = k1*x + b1;
            return QPointF(x,y);
        }
        // case: l1水平
        else
            return QPointF(x1,l1.p1.y());
    }
}

/**
 * @brief 求等分点
 * @param p1 直线上的参照点
 * @param p2 直线上的另一点
 * @param proprtion 所求点到参照点的距离是p1、p2距离的比例
 * @return
 */
QPointF MyRule::divide(QPointF p1, QPointF p2, qreal proprtion)
{
    qreal dx = p2.x()-p1.x(), dy = p2.y()-p1.y(),
          x = p1.x()+dx*proprtion, y = p1.y()+dy*proprtion;
    return QPointF(x,y);
}

/**
 * @brief 求交点
 * @param l1
 * @param l2
 * @param ok 无交点时赋值为false
 * @return 无交点时返回(0,0)
 */
QPointF MyRule::cross(Line l1, Line l2, bool *ok)
{
    qreal x11=l1.p1.x(), x21=l2.p1.x(), y11=l1.p1.y(), y21=l2.p1.y(),
          x12=l1.p2.x(), x22=l2.p2.x(), y12=l1.p2.y(), y22=l2.p2.y(),
          dx1 = x11-x12, dy1 = y11-y12, dx2 = x21-x22, dy2 = y21-y22;
    // case: l1竖直
    if(zero(dx1)){
        // case: l2水平
        if(zero(dy2))
            return QPointF(x11,y21);
        // case: l2竖直
        else if(zero(dx2)){
            if(ok != nullptr) *ok = false;
            return QPointF(0,0);
        }
        // case: l2倾斜
        else{
            qreal k2=dy2/dx2, b2=y21-x21*k2;
            return QPointF(x11,k2*x11+b2);
        }
    }
    // case: l1水平
    else if(zero(dy1)){
        // case: l2水平
        if(zero(dy2)){
            if(ok != nullptr) *ok = false;
            return QPointF(0,0);
        }
        // case: l2竖直
        else if(zero(dx2))
            return QPointF(x21,y11);
        // case: l2倾斜
        else{
            qreal k2=dy2/dx2, b2=y21-x21*k2;
            return QPointF((y11-b2)/k2,y11);
        }
    }
    // case: l1倾斜
    else{
        qreal k1=dy1/dx1, b1=y11-x11*k1;
        // case: l2水平
        if(zero(dy2))
            return QPointF((y21-b1)/k1,y21);
        // case: l2竖直
        else if(zero(dx2))
            return QPointF(x21,k1*x21+b1);
        // case: l2倾斜
        else{
            qreal k2=dy2/dx2, b2=y21-x21*k2, x=(b2-b1)/(k1-k2);
            return QPointF(x,k1*x+b1);
        }
    }
}

/**
 * @brief 求直线与曲线的交点
 * @param l
 * @param c
 * @param ok 无交点时返回false
 * @return 无交点时返回(0,0)
 */
QPointF MyRule::cross(Line l, Curve c, bool *ok)
{
    MyPainter painter = MyPainter();
    painter.curve(c.points, c.p1, c.p2);
    qreal t = 0, dt = 0.01;
    QPointF p1, p2;
    while(t < 0.999){
        p1 = painter.myPath->pointAtPercent(t);
        t += dt;
        p2 = painter.myPath->pointAtPercent(t);
        Line l1 = Line{ p1, p2 };
        bool existCross = true;
        QPointF crossPoint = cross(l, l1, &existCross);
        if(existCross)
            return crossPoint;
    }
    if(ok != nullptr)
        *ok = false;
    return QPointF(0, 0);
}

QPointF MyRule::cross(Curve c1, Curve c2, bool *ok)
{
    // todo
    return QPointF(0,0);
}

/**
 * @brief 连接2点的直线段
 * @param p1
 * @param p2
 * @return
 */
Line MyRule::line(QPointF p1, QPointF p2)
{
    Line l = {
        p1,p2
    };
    return l;
}

/**
 * @brief 圆顺各点生成曲线
 * @param points
 * @param ok
 * @return
 */
Curve MyRule::curve(QList<QPointF> points, bool* ok)
{
    if(points.length() < 3){
        if(ok != nullptr)
            *ok = false;
        info("圆顺方法的参数错误！");
        QPointF p = QPointF(0, 0);
        QList<QPointF> points;
        points << p << p << p;
        Curve c = { p, p, points };
        return c;
    }else{
        Curve c = { points.first(),
                  points.last(),
                  points };
        return c;
    }

}

/**
 * @brief 解析1句规则代码
 * @param code
 */
bool MyRule::parseCode(QString code)
{
    code = pretreat(code);
    if(code == "")
        return true;
    else if(code.left(3) == "规则 "){
        // todo
        return true;
    }
    QStringList names = getEntityNames(code);
    QString value = getValue(code);
    if(value == ""){
        QString type = getEntityType(code);
        if(type == ""){
            // case: 指示输出实体
            if(code.left(3) == "输出 "){
                if(names.length() == 1){
                    if(entityOut != "") info("输出实体被重复定义，\n以最后一次定义为准");
                    entityOut = names[0];
                    return true;
                }else info(code + "语法错误：\n输出实体名数目>1");
            }
            // case: 其他语句
            else info(code + "无法解析");
        }
        // case: 指示输入实体
        else if(code.left(3) == "输入 "){
            if(names.length() != 1) info(code + "语法错误：\n输入实体名数目!=1");
            else{
                if(getTypeOf(names[0]) == ""){
                    int i = 0;
                    while(!setInput(type,names[0]))
                        if(i < 3){
                            i++;
                            info("你输入的值不合法，请重新输入……");
                        }
                        else{
                            info("你输入的值不合法，退出");
                            return false;
                        }
                }
                return true;
            }
        }
        // case: 单纯的定义语句
        else{
            defineEntity(type,names[0]);
            return true;
        }
    }else{
        QStringList items = code.split("=",QString::SkipEmptyParts);
        if(items.length() == 2){
            QStringList nameLeft = getEntityNames(items[0]);
            if(nameLeft.length() != 1) info(code + "语法错误：\n等号左边的实体名数目!=1");
            else if(nameLeft[0] != names[0]) info(code + "实体名出错");
            else{
                QString name = names[0];
                QString type = getEntityType(items[0]);
                // case: 单纯的赋值语句
                if(type == ""){
                    assignEntity(name,value);
                    return true;
                }
                // case: 初始化赋值语句
                else{
                    defineEntity(type,name);
                    assignEntity(name,value);
                    return true;
                }
            }
        }else info(code + "语法错误：\n等号的数目>1");
    }
    return false;
}

/**
 * @brief 对单句code进行预处理
 * @param code
 * @return 处理之后的语句
 */
QString MyRule::pretreat(QString code)
{
    // 将中文标点 替换为 英文标点
    code.replace("（","(");
    code.replace("）",")");
    code.replace("，",",");
    // 将换行符去掉
    code.remove("\n");
    // 将负号前的空格去掉
    code.replace(" -","-");
    // 在负号前面加上"0"，如"(-1,-2-3)"变成"(0-1,0-2-3)"
    if(code[0]=='-')
        code = "0" + code;
    code.replace("(-","(0-");
    code.replace(",-",",0-");
    return code;
}

/**
 * @brief 读取代码中的实体名称
 * @param code
 * @return
 */
QStringList MyRule::getEntityNames(QString code)
{
    // 根据code中的“非字母、数字、下划线”部分，拆分成strList
    QStringList strList = code.split(QRegExp("\\W+"), QString::SkipEmptyParts);
    // 过滤出strList中以字母开头的字符串
    return strList.filter(QRegExp("^[a-z]|[A-Z]"));
}

/**
 * @brief 在code中获取实体类型
 * @param code
 * @return ""表示code中没有实体类型
 */
QString MyRule::getEntityType(QString code)
{
    for(int i=0;i<types.length();i++){
        int id = code.indexOf(types[i]), idAdd = id + types[i].length();
        if(id != -1 && (idAdd==code.length() || code[idAdd]==' ') && (id==0 || code[id-1]==' '))
            return types[i];
    }
    return "";
}

/**
 * @brief 在code中获取“值”字符串
 * @param code
 * @return ""表示code中没有赋值
 */
QString MyRule::getValue(QString code)
{
    int id = code.indexOf("=");
    if(id == -1)
        return "";
    else
        return code.mid(id+1).simplified();
}

/**
 * @brief 定义实体，将name作为键写入QMap，值为默认值
 * @param type
 * @param name
 */
void MyRule::defineEntity(QString type, QString name)
{
//    // 重名检查
//    if(getTypeOf(name) != ""){
//        info("语法错误：" + name + "被多次定义。\n以最后一次为准。");
//    }
    switch(types.indexOf(type)){
    case -1:
        info("无法定义实体：类型错误");
        break;
    case 0:
        params.insert(name,"0");
        break;
    case 1:
        points.insert(name,QPointF(0,0));
        break;
    case 2:
        lines.insert(name,Line{QPointF(0,0),QPointF(0,0)});
        break;
    case 3:
        paths.insert(name,"空路径");
        break;
    case 4:
        QPointF p = QPointF(0,0);
        QList<QPointF> pl;
        pl << p << p << p;
        curves.insert(name, Curve{p, p, pl});
    }
}

/**
 * @brief 实体赋值
 * @param name
 * @param value
 */
void MyRule::assignEntity(QString name, QString value)
{
    QMap<QString,QString>::iterator it1;
    it1 = params.find(name);
    if(it1 != params.end()){
        bool ok = true;
        qreal v = param(value, &ok);
        if(ok) *it1 = QString::number(v,'f',3); // 精确到0.001
        else *it1 = value;
        return;
    }
    QMap<QString,QPointF>::iterator it2;
    it2 = points.find(name);
    if(it2 != points.end()){
        *it2 = point(value);
        return;
    }
    QMap<QString,Line>::iterator it3;
    it3 = lines.find(name);
    if(it3 != lines.end()){
        *it3 = line(value);
        return;
    }
    it1 = paths.find(name);
    if(it1 != paths.end()){
        *it1 = path(value);
        return;
    }
    QMap<QString,Curve>::iterator it4;
    it4 = curves.find(name);
    if(it4 != curves.end()){
        *it4 = curve(value);
        return;
    }
}

/**
 * @brief 参数求值，字符串转换为qreal类型
 * @param value
 * @param ok bool指针 必要时赋值为false
 * @return
 */
qreal MyRule::param(QString value, bool *ok)
{
    QString s = pretreat(value.remove(' '));
    QMap<QString,QString>::iterator it = params.find(s);
    // case: 值为参数名
    if(it != params.end())
        return param(*it, ok);
    // case: 制板公式
    else if(s.contains(QRegExp("[+\\-\\*/]"))){
        // 将公式中的参数名转化为数值
        QStringList paramNames = s.split(QRegExp("[+\\-\\*/\\(\\)]"),QString::SkipEmptyParts);
        paramNames = paramNames.filter(QRegExp("^[a-z]|[A-Z]"));
        foreach(const QString &n, paramNames){
            it = params.find(n);
            if(it != params.end()){
                QString otherParamValue = *it; // it.value() == *it ?
                otherParamValue = QString::number(param(otherParamValue));
                s = s.replace(n, otherParamValue);
            }else{
                info("公式"+value+"中包含未知符号！");
                if(ok != nullptr) *ok = false;
                return 0;
            }
        }
        // 再对公式求值
        return calculate(s, ok);
    }
    // case: 点的横纵坐标
    else if(s.contains(".横坐标") || s.contains(".纵坐标")){
        int i = s.lastIndexOf('.');
        bool b = s[i+1] == "横";
        s = s.left(i);
        QPointF p = point(s, ok);
        if(b) return p.x();
        else return p.y();
    }
    // case: 值为单个数值
    else{
        return s.toDouble(ok);
    }
}

/**
 * @brief 点值解析，字符串转换为QPointF类型
 * @param value
 * @param ok bool指针 必要时赋值为false
 * @return
 */
QPointF MyRule::point(QString value, bool *ok)
{
    value = value.simplified();
    QStringList numbers = value.split(QRegExp("\\(|\\)|,"),QString::SkipEmptyParts);
    if(numbers.length() != 2 || value[0] != '('){
        // case: 赋值为 另一个点
        if(getTypeOf(value) == types[1])
            return points[value];
        else{
            int idFunc = 0;
            for(;idFunc < pFuncs.length(); idFunc++){
                if(value.left(pFuncs[idFunc].length()) == pFuncs[idFunc]
                   && value[pFuncs[idFunc].length()] == '(')
                    break;
            }
            if(idFunc == pFuncs.length())
            {
                // case: 赋值为 直线的端点
                if(value.contains("端点") && value.contains(".")){
                    numbers = value.split(".");
                    if(numbers.length() == 2 && getTypeOf(numbers[0]) == types[2])
                        return endPoint(lines[numbers[0]],numbers[1],ok);
                }
                // case: 调用自定义规则 赋值
                else if(value.contains("(") && value.contains(")")){
                    int i1 = value.indexOf('('), i2 = value.lastIndexOf(')');
                    QString ruleName = numbers[0],
                            enIn = value.mid(i1+1,i2-i1-1).simplified();
                    return pointByRule(ruleName, enIn, ok);
                }
            }
            // case: 调用基本约束方法 赋值
            else
                return pFunc(value, idFunc, ok);
        }
    }
    // case: 采用 ( , ) 赋值
    else{
        qreal x = param(numbers[0],ok),y = param(numbers[1],ok);
        return QPointF(x,y);
    }
    info("点值"+value+"解析出错");
    if(ok != nullptr)
        *ok = false;
    return QPointF(0,0);
}

/**
 * @brief 直线值解析，字符串转换为Line结构体
 * @param value
 * @param ok bool指针 必要时赋值为false
 * @return
 */
Line MyRule::line(QString value, bool *ok)
{
    value = value.simplified();
    QStringList p = value.split("连接",QString::SkipEmptyParts);
    if(p.length() != 2){
        // case: 赋值为 另一条直线
        if(getTypeOf(value) == types[2])
            return lines[value];
        // case: 调用自定义规则 赋值
        else if(value.contains("(") && value.contains(")")){
            int i1 = value.indexOf('('), i2 = value.lastIndexOf(')');
            QString ruleName = value.left(i1),
                    enIn = value.mid(i1+1,i2-i1-1).simplified();
            return lineByRule(ruleName, enIn, ok);
        }
        info("直线值"+value+"解析出错");
        if(ok != nullptr)
            *ok = false;
        return line(QPointF(0,0),QPointF(0,0));
    }
    // case: 连接两点赋值
    QPointF p1 = point(p[0], ok), p2 = point(p[1], ok);
    return line(p1, p2);
}

/**
 * @brief 路径值字符串解析
 * @param value
 * @param ok bool指针 必要时赋值为false
 * @return
 */
QString MyRule::path(QString value, bool *ok)
{
    QMap<QString,QString>::iterator it = paths.find(value);
    // case: 值为参数名
    if(it != paths.end())
        return path(*it, ok);
    // case: 值为字面量（字符串）
    else {
        return value;
    }
}

/**
 * @brief 曲线值解析
 * @param value
 * @param ok bool指针 必要时赋值为false
 * @return
 */
Curve MyRule::curve(QString value, bool *ok)
{
    value = value.simplified();
    QStringList p = value.split("圆顺",QString::SkipEmptyParts);
    if(p.length() == 1){
        // case: 赋值为 另一条曲线
        if(getTypeOf(value) == types[4])
            return curves[value];
        // case: 调用自定义规则 赋值
        else if(value.contains("(") && value.contains(")")){
            int i1 = value.indexOf('('), i2 = value.lastIndexOf(')');
            QString ruleName = value.left(i1),
                    enIn = value.mid(i1+1,i2-i1-1).simplified();
            return curveByRule(ruleName, enIn, ok);
        }
        info("曲线值"+value+"解析出错");
        if(ok != nullptr)
            *ok = false;
        QList<QPointF> pl;
        return curve(pl);
    }
    // case: 圆顺各点赋值
    QList<QPointF> pl;
    int len = p.length();
    for(int i = 0; i < len; ++i){
        pl.append(point(p[i], ok));
        if(ok != nullptr && *ok == false){
            QString s = "圆顺方法的第" + QString::number(i) + "个点值解析出错！";
            info(s);
            QList<QPointF> pl;
            return curve(pl);
        }
    }
    return curve(pl, ok);
}

/**
 * @brief 查QMap，获取实体name对应的类型
 * @param name
 * @return 返回""表示没有找到该实体
 */
QString MyRule::getTypeOf(QString name)
{
    QMap<QString,QString>::iterator it1;
    it1 = params.find(name);
    if(it1 != params.end())
        return types[0];
    QMap<QString,QPointF>::iterator it2;
    it2 = points.find(name);
    if(it2 != points.end())
        return types[1];
    QMap<QString,Line>::iterator it3;
    it3 = lines.find(name);
    if(it3 != lines.end())
        return types[2];
    it1 = paths.find(name);
    if(it1 != paths.end())
        return types[3];
    QMap<QString,Curve>::iterator it4;
    it4 = curves.find(name);
    if(it4 != curves.end())
        return types[4];
    return "";
}

/**
 * @brief 调用自定义规则，会弹框要求用户赋值输入实体
 * @param f 规则文件的路径
 * @param in 输入实体的代码
 * @param parent 发起call的MyRule对象指针
 * @return 输出实体的名称。""表示失败
 */
QString MyRule::callRule(QString f, QString in, MyRule* parent)
{
    // 解析输入实体
    if(in != "" && parent != nullptr){
        parentRule = parent;
        // 将输入实体（的名称）加入entitiesIn队列
        entitiesIn = pretreat(in).split(',');
    }
    // 打开规则文件
    QFile rule(f);
    if(!rule.open(QIODevice::ReadOnly|QIODevice::Text)){
        info("打开文件失败");
        return "";
    }
    // 逐句解析规则代码
    while (!rule.atEnd()) {
        QTextCodec *codec= QTextCodec::codecForName("utf8");
        if(!parseCode(codec->toUnicode(rule.readLine()))){
            info("规则文件解析出错\n"+f);
            break;
        }
    }
    rule.close();
    // 返回输出实体名
    if(getTypeOf(entityOut) != "")
        return entityOut;
    else
        return "";
}

/**
 * @brief 调用自定义规则获取点
 * @param f 规则的名称
 * @param in 输入实体的名称，形式为"name1,name2,..."（目前只能填写名称，绝不能出现","）
 * @param ok 失败时赋值为false
 * @return
 */
QPointF MyRule::pointByRule(QString f, QString in, bool *ok)
{
    QPointF p = QPointF(0,0);
    QString type = "";
    f = findRulePath(f);
    MyRule *rule = new MyRule(f);
    QString name = rule->callRule(f, in, this);
    if(name != ""){
        type = rule->getTypeOf(name);
        if(type == types[1])
            p = rule->points[name];
    }
    if(name == "" || type != types[1])
        if(ok != nullptr) *ok = false;
    delete rule;
    return p;
}

/**
 * @brief 调用自定义规则获取直线
 * @param f 规则的名称
 * @param in 输入实体的名称，形式为"name1,name2,..."（目前只能填写名称，绝不能出现","、"连接"）
 * @param ok 失败时赋值为false
 * @return
 */
Line MyRule::lineByRule(QString f, QString in, bool *ok)
{
    Line l = { QPointF(0,0), QPointF(0,0) };
    QString type = "";
    f = findRulePath(f);
    MyRule *rule = new MyRule(f);
    QString name = rule->callRule(f, in, this);
    if(name != ""){
        type = rule->getTypeOf(name);
        if(type == types[2])
            l = rule->lines[name];
    }
    if(name == "" || type != types[2])
        if(ok != nullptr) *ok = false;
    delete rule;
    return l;
}

/**
 * @brief 调用自定义规则获取曲线
 * @param f 规则的名称
 * @param in 输入实体的名称，形式为"name1,name2,..."（目前只能填写名称，绝不能出现","、"圆顺"）
 * @param ok 失败时赋值为false
 * @return
 */
Curve MyRule::curveByRule(QString f, QString in, bool *ok)
{
    QList<QPointF> points;
    QPointF point = QPointF(0,0);
    points << point << point << point;
    Curve c = { point, point, points };
    QString type = "";
    f = findRulePath(f);
    MyRule *rule = new MyRule(f);
    QString name = rule->callRule(f, in, this);
    if(name != ""){
        type = rule->getTypeOf(name);
        if(type == types[4])
            c = rule->curves[name];
    }
    if(name == "" || type != types[4])
        if(ok != nullptr) *ok = false;
    delete rule;
    return c;
}

/**
 * @brief 根据自身规则文件生成输出实体的绘图路径
 * @return
 */
MyPainter MyRule::drawPath()
{
    return drawPath(callRule(file));
}

/**
 * @brief 根据实体名name生成实体的绘图路径
 * @param name
 * @return
 */
MyPainter MyRule::drawPath(QString name)
{
    MyPainter path;
    if(name == "")
        return  path;
    QString type = getTypeOf(name);
    if(type == ""){
        info("无法绘制：找不到"+name);
        return path;
    }
    switch(types.indexOf(type)){
    case 1:
        return drawPath(points[name]);
    case 2:
        return drawPath(lines[name]);
    case 3:
        return drawPathByCode(paths[name]);
    case 4:
        return drawPath(curves[name]);
    default:
        info("无法绘制"+name);
        return path;
    }
}

MyPainter MyRule::drawPath(QPointF point)
{
    QPainterPath p;
    MyPainter mp;
    // 画一个×
    qreal x = point.x(), y = point.y();
    p.moveTo(QPointF(x-5,y-5));
    p.lineTo(QPointF(x+5,y+5));
    p.moveTo(QPointF(x-5,y+5));
    p.lineTo(QPointF(x+5,y-5));

    mp.myPath->addPath(p);
    // todo: mp.myData中加入点
    return mp;
}

MyPainter MyRule::drawPath(Line line)
{
    QPainterPath p;
    MyPainter mp;
    p.moveTo(line.p1);
    p.lineTo(line.p2);
    mp.myPath->addPath(p);
    // todo: mp.myData中加入直线
    return mp;
}

MyPainter MyRule::drawPath(Curve c)
{
    // todo
}

/**
 * @brief 根据path的代码生成路径(使用MyPainter类）
 * @param path
 * @return
 */
MyPainter MyRule::drawPathByCode(QString path)
{
    MyPainter painter;
    painter.parseCode(this, path);
    return painter;
}

/**
 * @brief 判断r是否可以视作0
 * @param r
 * @return
 */
bool MyRule::zero(qreal r)
{
    if(r<0.0001 && r>-0.0001) return true;
    else return false;
}

/**
 * @brief 判断两个实数是否相等
 * @param r1
 * @param r2
 * @return
 */
bool MyRule::equal(qreal r1, qreal r2)
{
    if(zero(r1 - r2)) return true;
    else return false;
}
