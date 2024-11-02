# easyDualMonitor
双显示器工具

1, 按Win键时 打开鼠标所在显示器的开始菜单

2, 在鼠标所在显示器打开新的窗口

# type: "hotkey" param支持 
    static etd::map<etd::string, BYTE> keyMap = {
        {"backspace", VK_BACK},   
        {"tab", VK_TAB},   
        {"enter", VK_RETURN},   
        {"shift", VK_SHIFT},   
        {"ctrl", VK_CONTROL},   
        {"alt", VK_MENU},   
        {"pause", VK_PAUSE},   
        {"capslock", VK_CAPITAL},   
        {"escape", VK_ESCAPE},   
        {"space", VK_SPACE},   
        {"pageup", VK_PRIOR},   
        {"pagedown", VK_NEXT},   
        {"end", VK_END},   
        {"home", VK_HOME},   
        {"left", VK_LEFT},   
        {"up", VK_UP},   
        {"right", VK_RIGHT},   
        {"down", VK_DOWN},   
        {"insert", VK_INSERT},   
        {"delete", VK_DELETE},   
        {"del", VK_DELETE},   
        {"f1", VK_F1},   
        {"f2", VK_F2},   
        {"f3", VK_F3},   
        {"f4", VK_F4},   
        {"f5", VK_F5},   
        {"f6", VK_F6},   
        {"f7", VK_F7},   
        {"f8", VK_F8},   
        {"f9", VK_F9},   
        {"f10", VK_F10},   
        {"f11", VK_F11},   
        {"f12", VK_F12},   
        {"numlock", VK_NUMLOCK},   
        {"scrolllock", VK_SCROLL},   
        {"printscreen", VK_SNAPSHOT},   
        {"win", VK_LWIN},  // 左侧 Windows 键
        {"apps", VK_APPS}, // 应用程序键
    };