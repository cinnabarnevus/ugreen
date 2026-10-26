# Mini54 新功能接入规范

**目的**：让新增功能**读起来和现有代码是一个样子**（可读性），并**与既有架构保持一致**（挂到现有调度器上，而不是另起一套）——能照抄现有模块的写法，就不要发明新写法。

新功能 = `function/inc/xxx.h` + `function/src/xxx.c`，通过下面四个接入点挂进现有调度器；构建以 **Keil AC5** 为准（`IAR/*.ewp` 已过期，未同步 led/adc_key/power_control）。

## 铁律

1. **不改骨架**：`main.c` 主循环和 `core/core.c` 调度器不塞模块逻辑，只挂注册点。
2. **静态分配**：RAM 共 2KB，已用 1336B，栈余 ~700B。不用 `malloc`，大数组先论证。
3. **中断里只置标志**，重活交给 `TaskWorker`（范例 `update.c:62` → `update.c:30`）。
4. **算 Flash 账**：可用 15872B（`0x0000–0x3DFF`），当前 14856B，余 ~1KB。不写 `.name`、不引 `float`（软浮点链 430B）、不用 `sprintf`/`malloc`、`DEBUG` 格式串要短。
5. **跨模块不直接互相调用**，用 `volatile` 标志 / 请求位解耦（范例 `pending_power_request` 的 `NV_REQ_POWERON`/`NV_REQ_POWEROFF`，`pm.c` ↔ `power_control.c` 靠它交互）。
6. **`.h` 是接口与声明，`.c` 是实现与定义**：跨模块使用的类型（`struct`/`enum`/`typedef`）、函数与变量声明，放模块自己的 `function/inc/xxx.h`；函数体、变量实体、只给本模块用的类型/宏/`static` 辅助函数，放 `xxx.c`。判断标准只有一条：**有没有跨模块使用**。头文件里不放实体（`ir.h:63` 的数组、`pm.h:25-34` 的 `static inline`），也不要在 `.c` 里 `extern` 临时声明别人的接口。正例 `adc_key.c:86`（内部类型 `adc_key_win_t` 留在 `.c`），反例 `led.h:10,20`（只被 `led.c:57` 用的 `led_color_t`/`led_ctrl_t` 进了公共头）。

## 四条接入路径

| 需求 | 接口 | 上下文 | 频率 |
|---|---|---|---|
| 状态推进 / 消费标志位 | `RegistTaskWorker` | 主循环 | 不定；灯刷新那圈被拖到 ~1.2ms |
| 周期采样 / 超时（≥10ms 粒度） | `RegistAlarmEventCallback`+`UpdateAlarmEventTime` | SysTick 中断（最低优先级 3） | 10ms |
| 主机写寄存器（MCU 收命令） | `RegistTransBufferCallback` | I2C 中断（优先级 0） | 事件 |
| 主机读寄存器（MCU 上报） | **无注册接口**，改 `core.c` switch | I2C 中断 | 事件 |

## 注册机制共性（本节规则对三个注册函数都成立）

- **头插链表 → 后注册的先执行**（`core.c:118/125/141` 都是 `list_add`）。当前实际序：任务 `UpdateTaskWorker` → `PowerManageTaskWorker`；寄存器回调链 `pm` → `wol` → `update`。
- **`char *name` 会被覆盖**：注册函数把整个结构体当 `list_head` 头插，偏移 0 的 `name` 被写成 `next` 指针。**不读、不写**（写了就是约 13B 的死字符串）。
- 注册对象必须**全局/static**，不能是函数内局部变量。
- **中断上下文规则**（定时回调 = SysTick，收发寄存器 = I2C）：只采样/只置标志。禁止 `printf`/`DEBUG`（UART 115200 阻塞 ~87µs/字符，会反噬时基或拖垮 I2C 时序）、禁止 `Delay*()`、禁止在回调里增删链表节点（调度器正在遍历）。

## 1. `RegistTaskWorker(struct task_struct *)` — 轮询任务

`ScheduleTaskWorkerLooper()`（`core.c:148`）每圈顺序跑完链表，由 `main.c:55` 调用。返回 `int` **被忽略**，约定 `return 0`。

- 不能阻塞：这是全系统唯一的非中断上下文，卡住它 = 卡住 I2C 响应和按键扫描。不许 `Delay*()`、不许 `printf` 刷屏。
- `ScheduleTaskWorker()` / `CancelTaskWorker()` 只有声明（`core.h:170-171`）**没有实现**，调用会链接失败。

## 2. `RegistAlarmEventCallback()` + `UpdateAlarmEventTime(item, ms, enable)` — 软件定时

`SysTick_Handler`（`core.c:109`）每 10 个 1ms tick 调 `TrigerAlarmEventCallback(10)`；倒计时减到 `<= ms` 触发并重装，`enabled == 0` 跳过。

- **周期取 10 的整数倍**（25 会实际 30ms 触发）；`0` = 每次进来都触发。
- 注册时 `enabled` 给 0，硬件先备好，初始化末尾再 `UpdateAlarmEventTime(..., 1)` 打开（范例 `adc_key.c:255`）。

## 3. `RegistTransBufferCallback()` — 主机写寄存器

链路：I2C 从机地址 **0x15**（7bit）→ 主机写 `[reg][value]` 2 字节 → `I2C_IRQHandler` 状态 `0xA0`（`core.c:81`）→ `TransformBufferCallback()`（`core.c:182`）→ 按注册顺序遍历，**第一个 `return 0` 的赢**。

回调原型固定 `int (*callback)(uint8_t reg, uint8_t value)`：`0` = 已处理并终止链，`-1` = 不是我的、继续下一个。无人处理 = **静默丢弃**。

- **先 `item.trans = &Callback` 再注册**（`TransformBufferCallback` 不判空）。
- **一个寄存器号只能一个模块负责**：否则后注册者截胡，前者永远收不到。
- 写 Flash 只在值变化时（`if (旧 != 新)`）：`FlashRegistUpdate()` 擦写整个 512B 页、耗时数毫秒，而这发生在中断里。
- 一次事务只认 2 字节；多字节量拆成多个寄存器（范例 MAC 拆 6 个）。

## 4. 主机读寄存器 — 无注册接口

链路：`0xA8` → `SlaveI2cTransformRegistToHost()`（`core.c:36`）→ **`GetRegistBufferValue()` 的 `switch(reg)`**（`core.c:200`）。

- 新寄存器**必须在那里加 `case`，否则主机恒读回 0**。已存在的只写寄存器：`0x0E`、`0x23`、`0x41`、`0x60`。
- `reg > MAX_REG`（100）返回 0；一次只回 1 字节，>8bit 拆 `_H`/`_L`（`MCU_ADC_KEY_RAW_H/L` 范例，H 只取高 2 位）。
- 读路径零副作用；值来自 `RegistBuffer[]` 或模块 getter（`adc_key_get()`、`sata_pwr_state`）。
- **寄存器号是主机侧协议：只能新增，不能重排、不能复用旧号。**

## 5. 新增模块 checklist

1. `function/inc/xxx.h`（guard `__XXX_H__`）+ `function/src/xxx.c`；include 顺序 `"core.h"` → `"board.h"` → `"xxx.h"`。对外接口全部在 `.h` 声明，内部辅助函数 `static` 定义在 `.c`。
2. 定义 static 状态 + 全局注册对象（不写 `.name`）+ 回调（签名照抄 §1–§3）。
3. 写 `void xxx_init(void)`：先硬件、后注册、最后 arm 定时器。
4. 在 `core_init()`（`core.c:432`）里调用 `xxx_init()`。依赖 Flash 配置的必须在 `InitFlashAndRegist()` 之后；GPIO 方向 `board_init()` 已做完。
5. 新增主机寄存器：`core.h` 的 `MCU_REGIST` 加编号 → `core.c GetRegistBufferValue()` 加读 `case` → 自己的回调加写 `case`。
6. 新增持久化配置：字段**只能追加到 `struct FlashRegist` 末尾**（`core.h:93`；插中间会让老机器 flash 数据整体错位），默认值写进 `InitDefaultFlashRegistBuffer()`（`core.c:375`），读取端兜底老数据（范例 `led.c:189` 对 `blinkTimer == 0/0xFF`）。落盘只用 `FlashRegistUpdate()`；分区 512B，`MAX_FLASH_REG = 22` 字。
7. 把 `xxx.c` 加进 `KEIL/mini54-application.uvprojx` 的 `function` 组。⚠️ **该文件被 skip-worktree，改动 git 看不见、永远不会被提交**（`git ls-files -v | grep '^S'` 可查），要么 `git update-index --no-skip-worktree` 后提交，要么显式告知。
8. 编译后核对 `KEIL/Listings/mini54-application.map` 的 `Total ROM Size ≤ 15872`。⚠️ `config/mini54-application.sct` 允许链接器越过 `0x3E00` 且不报警，越界 = 固件第一次写配置擦掉自己的代码段。

## 6. 例外：不走注册点的情况

- 需要远快于 10ms 的轮询 → `main.c` 主循环加一行 + 头文件写明理由（范例 `adc_key_debug()`）。
- `power_control.c` 的 1ms 虚拟定时器 `vtimer_hander()`（5 个槽，`[4]` 已被 `mainloop_handler` 占用，由 SysTick 每 tick 驱动 → `power_handler()`）属于既有体系，**不要另起第二套时基**。

## 7. 寄存器编号

- 已占用：`0x01,0x02,0x11`(VID/PID/VER)、`0x09,0x0D,0x0F,0x1E,0x20,0x2E,0x30`（pm 写）、`0x27–0x2C` MAC（wol 写）、`0x12` REBOOT_FLAG、`0x51–0x53` DEBUG_*、`0x55–0x59` LED（pm 写）、`0x61–0x63` ADC key（只读）。
- 空闲可用（≤100）：`0x03–0x08`、`0x0A–0x0C`、`0x0E`、`0x10`、`0x13–0x1D`、`0x1F`、`0x21–0x22`、`0x24–0x26`、`0x2D`、`0x2F`、`0x31–0x40`、`0x42–0x50`、`0x54`、`0x5A–0x5F`、`0x64+`。

## 8. 历史包袱与编码坑

- `ir.c`、`watchdog.c` **不在编译列表**：`ir.c` 引用了 `struct FlashRegist` 中不存在的字段（`IrWake` 等），`watchdog.c` 的 `WatchdogAlarmItem` 与 `pm.c` 重名。不要参照、不要启用。
- **声明/实现错位的既有写法，新代码不要模仿**：`core.c:108` 直接在 `.c` 里写 `extern void vtimer_hander(void);`；`power_control.c` 至今没有自己的 `.h`，它的 `pending_power_request`、`get_poweron_times()` 声明寄生在 `pm.h:22-23`，而 `power_control.c:47-53` 又在 `.c` 里自声明了一批原型；反方向的两个例子是 `ir.h:63` 在头文件里**定义**了 `static struct ir_button mIrbutton[]`，以及 `led.h:10,20` 把只被 `led.c:57` 用到的 `led_color_t`/`led_ctrl_t` 放进了公共头（对比 `adc_key.c:86` 把内部类型 `adc_key_win_t` 留在 `.c`，这才是对的）。
- 全仓库 `.c/.h` 是 **CRLF**。`board.c`、`pm.c`、`power_control.c`、`adc_key.c` 的中文注释是 **GBK/ISO-8859-1**，用会整文件重写的工具改会毁掉注释；新文件写 UTF-8。

## 9. 模板与命名

命名照抄现有代码——同一件事在 6 个模块里长一个样子，读起来才好对照。下面每条都是全项目核对过的**多数派**，不是理想规范：

- **回调与任务**：`XxxTransCallback` / `XxxTaskWorker` / `XxxAlarmCallback`（CamelCase，全项目零例外）
- **注册对象**：`XxxTask` / `XxxTransItem` / `XxxCallback` / `XxxAlarmItem`
- **初始化**：`xxx_init()`（`pm_task_init` / `update_task_init` 是历史写法，新模块不要再带 `_task_`）
- **其余对外函数**：小写下划线（`adc_key_get` / `get_host_power_status` / `sata_power_control`）；框架 API 是 CamelCase（`RegistTaskWorker` / `GetRegistBufferValue`），不改也不混
- **宏与寄存器**：全大写下划线；主机寄存器 `MCU_XXX`、调试 `DEBUG_XXX`；引脚宏一律放 `board.h`（`HOST_XXX_IN` / `XXX_OUT`）
- **类型**：`typedef ... xxx_t`（`led_mode_t` / `adc_key_t` / `power_event_t`，`HOST_STATE` 是无 `_t` 的老写法）；枚举成员全大写带模块前缀（`LED_MODE_ON` / `ADC_KEY_NONE`）
- **变量**：模块内部 `static` 小写下划线；跨模块导出的全局同风格（`host_status` / `sata_pwr_state` / `pending_power_request`），**不加 `g_` 前缀**（`g_led_mode` 是全项目唯一孤例，别学）
- **不用匈牙利前缀**：`u8/u16/u32` 只存在于 `Library/StdDriver`，应用层 0 处，统一 `uint8_t` / `uint32_t`
- **历史混写不要当模板**：`RGB_SendData`（下划线+Camel 混）、`PowerOnhostBoard`、`pwr_btn_Anti_shake`、`WDT_Feed`、`vtimer_hander`（拼写应为 handler）

```c
/* function/src/demo.c */
#include "core.h"
#include "board.h"
#include "demo.h"

struct task_struct DemoTask = {              /* 不写 .name */
	.function = DemoTaskWorker,
};
struct TransBufferItem DemoTransItem;
struct TransBufferCallBack DemoCallback = {
	.callback = DemoTransCallback,
};
struct timer_struct DemoAlarmItem = {
	.enabled  = 0,                            /* 末尾用 UpdateAlarmEventTime 打开 */
	.function = DemoAlarmCallback,
};

static volatile uint8_t demo_state, demo_pending;

int DemoTaskWorker(struct task_struct *__this)   /* 重活在这里；不阻塞，return 0 */
{
	/* __this->name 已被注册函数覆盖成链表指针，禁止访问 */
	if (demo_pending) { demo_pending = 0; /* ... */ }
	return 0;
}

int DemoAlarmCallback(struct timer_struct *__this)  /* SysTick 中断：只采样/置标志 */
{
	demo_state = DemoRead();
	demo_pending = 1;
	return 0;
}

int DemoTransCallback(uint8_t reg, uint8_t value)   /* I2C 中断：只落状态/置标志 */
{
	int ret = -1;                                    /* 默认：不是我的，继续往后传 */
	switch (reg) {
	case MCU_DEMO_CTRL:
		demo_state = value;
		demo_pending = 1;
		ret = 0;                                     /* 已处理，责任链终止 */
		break;
	default:
		break;
	}
	return ret;
}

uint8_t demo_state(void) { return demo_state; }      /* 供 core.c 读寄存器 switch 用 */

void demo_init(void)
{
	demo_state = demo_pending = 0;
	/* 先硬件，后注册 */
	DemoTransItem.trans = &DemoCallback;             /* 必须先赋值再注册 */
	RegistTransBufferCallback(&DemoTransItem);
	RegistTaskWorker(&DemoTask);
	RegistAlarmEventCallback(&DemoAlarmItem);
	UpdateAlarmEventTime(&DemoAlarmItem, DEMO_SCAN_MS, 1);   /* 10 的整数倍 */
}
```

配套：`core.c` 的 `core_init()` 加 `demo_init();`；`core.h` 的 `MCU_REGIST` 加 `MCU_DEMO_CTRL = 0x64,`；`core.c` 的 `GetRegistBufferValue()` 加 `case MCU_DEMO_CTRL: value = demo_state(); break;`；`uvprojx` 的 `function` 组加 `demo.c`。
