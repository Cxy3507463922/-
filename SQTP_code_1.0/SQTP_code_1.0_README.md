# SQTP_code_1.0_README

这个代码是可以直接实现的完整效果的，但操作还只能在服务器电脑的浏览器网页上实现。

1. esp32文件夹里面有些东西是VScode的Platform OI生成的内容（有一些path是自己电脑上的绝对路径，不能直接下载）。所以直接打开VScode，用Platform OI创建一个Project，把src下的main.cpp的内容复制替换，然后再在include文件夹下创建一个config.h，这个头文件里面的WIFI名称和密码以及服务器地址需要替换。
2. server文件夹里的代码我是用Pycharm写的（VScode应该也行），我在pycharm里创建了三个PY文件(main, database, config)，以及一个templates目录（这个名字不能改）（目录里面是4个HTML文件，index,action,error,status），其他文件也是自动生成的，可能会与自己电脑的路径有关。需要用到flask, json, request 等库（可以看到报错再下载）。
3. 目前需要有线连接