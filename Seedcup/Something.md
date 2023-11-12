### 几点说明  
> C++Client为cppsdk客户端，相比GitHub上增加了function.cpp,fuction.h,更改了parse.h,main.cpp。   

>Server&PyClient为server服务器及python客户端。python客户端方便调试，可手动操作，略微更改了ui.py的显示内容。

>cppsdk已写好网络通信协议等相关内容，将来开发的代码**原则上只需要修改function.cpp即可**。 

>目前已实现的功能是躲避炸弹，采用了A*算法，但仍有不少算法上的问题。