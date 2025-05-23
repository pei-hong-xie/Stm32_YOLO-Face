#STM32H750 视频处理与 YOLO 神经网络部署项目
#项目概述
本项目基于 STM32H750 微控制器平台，实现了一个集成式视频处理系统。系统利用 OV2640 摄像头采集实时视频数据，通过 DCMI 接口将数据高速传输到 MCU 内部；接收到的数据经过预处理后，利用通过 STM32CubeIDE 和 X-CUBE-AI 部署的 YOLO 神经网络进行目标检测与识别；随后，处理后的图像与检测结果利用 SPI 接口传输至 LCD 显示屏，最终实现实时视频显示和目标检测展示。

特性

- 硬件平台：STM32H750 微控制器

- 摄像头采集：使用 OV2640 摄像头，通过 DCMI 接口采集视频数据

- 高速数据传输：采用 DMA 技术实现数据的高速传输

- 神经网络推理：使用 STM32CubeIDE 和 X-CUBE-AI 工具链部署 YOLO 模型进行图像目标检测

- 图像显示：通过 SPI 总线将数据传输至 LCD，实现实时视频显示

图像预处理：

缩放算法（双线性插值），将摄像头采集的 240×280 图像转换为适合神经网络输入的 56×56 数据（或其他大小，根据需要）

神经网络推理：
通过 STM32CubeAI 生成的 C 模型，部署 YOLO 神经网络
>相关网络模型可以在 https://github.com/imuncle/yoloface-50k 下载

>本项目参考 https://github.com/imuncle/yoloface-50k
