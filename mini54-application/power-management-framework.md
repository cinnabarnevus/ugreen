# Mini54 电源管理框架

> 适用分支：`INJANS06-LL`（V04）
> 芯片：Nuvoton Mini54（Cortex-M0，12MHz XTAL，2KB RAM，APROM 实际可用 15KB）
> 定位：Mini54 作为主机的 SMC（System Management Controller），负责主机的上电/关电/复位时序、电源键、WOL、RTC 唤醒、SATA 电源、状态灯、主机看门狗和固件升级。

---

## 一句话概括

电源管理采用 **双层状态机**：

- **`pm.c` = 策略层**：决定“什么时候该开 / 该关 / 该复位”；
- **`power_control.c` = 执行层**：按毫秒级时序真正拉/放各条电源轨。

两者**不直接互相调用**，只通过 `pending_power_request` 请求位、GPIO 回读和 `RegistBuffer` 字段握手。其余模块（WOL、LED、ADC 按键、升级）都挂在 `core.c` 提供的任务/定时器/寄存器骨架上。

---

## 1. 分层架构

### 1.1 文件职责

| 层 | 文件 | 职责 |
|---|---|---|
| 板级 | `board/board.c`、`board/board.h` | 12MHz XTAL 时钟、GPIO 方向、I2C 从机、引脚宏；`get_host_power_status()` 从 P3.2/P5.4 读 SoC 电源状态 |
| 基础设施 | `core/core.c`、`core/core.h` | 主循环任务链表、10ms 软定时器、I2C 从机寄存器读写、`FlashRegist` 持久化、`Delay*()` |
| **策略层** | `function/src/pm.c`、`function/inc/pm.h` | `host_status` 状态机；按键/RTC/WOL/PWR_GOOD 中断；主机看门狗；`PowerOnhostBoard()` / `PowerDownHostBoard()` / `ResetHostBoard()` |
| **执行层** | `function/src/power_control.c` | `power_event` 状态机；5 个 1ms 虚拟定时器；驱动 `HOST_POWER_ENABLE` / `VIN_PWR_ON` / `PMIC_EN0` |
| 外设 | `function/src/wol.c` | MDIO 位操作 + 各 PHY 的 magic packet 配置 |
| 外设 | `function/src/led.c` | P2.2 GRB 软件时序、灯效、Timer1 10ms tick |
| 外设 | `function/src/adc_key.c` | P1.5/AIN5 三键电阻梯扫描 |
| 外设 | `function/src/update.c` | 主机命令跳 LDROM loader |

未参与编译的代码：

- `function/src/ir.c`：引用了 `struct FlashRegist` 中不存在的字段，且 `IR_TIMER` 与 TIMER1 冲突；
- `function/src/watchdog.c`：`WatchdogAlarmItem` 与 `pm.c` 重名；
- `board/board.c` 中的硬件 `WDT_Init()` 被注释，`WDT_Feed()` 无人调用。

### 1.2 架构图

```text
┌─────────────────────────────────────────────────────────────┐
│                          main.c                             │
│   board_init() → core_init() → while(1)                     │
│       ScheduleTaskWorkerLooper() + RGB_Process()            │
└───────────────────────────┬─────────────────────────────────┘
                            │
        ┌───────────────────┴───────────────────┐
        │              core/core.c              │
        │  任务链表 / 10ms 闹钟 / I2C 寄存器    │
        │  FlashRegist / Delay*()               │
        └───────┬───────────────┬───────────────┘
                │               │
     ┌──────────┴──────┐   ┌────┴───────────────┐
     │  pm.c 策略层     │   │ power_control.c    │
     │  host_status     │   │ power_event         │
     │  主循环          │   │ SysTick 1ms         │
     └──────────┬──────┘   └────┬───────────────┘
                │               │
                └───────┬───────┘
                        │ pending_power_request / GPIO / RegistBuffer
        ┌───────────────┴───────────────────────────────┐
        │  wol.c   led.c   adc_key.c   update.c         │
        └───────────────────────────────────────────────┘
```

### 1.3 模块接入点（四条路径）

| 需求 | 接口 | 执行上下文 | 频率 |
|---|---|---|---|
| 状态推进 / 消费标志位 | `RegistTaskWorker()` | 主循环 | 每圈 |
| 周期采样 / 超时 | `RegistAlarmEventCallback()` + `UpdateAlarmEventTime()` | SysTick 中断（最低优先级 3） | 10ms 粒度 |
| 主机写寄存器 | `RegistTransBufferCallback()` | I2C 中断（优先级 0） | 事件 |
| 主机读寄存器 | 无注册接口，改 `core.c` 的 `GetRegistBufferValue()` | I2C 中断 | 事件 |

注册采用头插链表，因此**后注册的先执行**。中断上下文只置标志/采样，重活交给主循环任务。

---

## 2. 启动与运行时骨架

### 2.1 启动顺序

```text
main()
 ├─ board_init()
 │    ├─ SYS_Clock()：12MHz XTAL，UART/I2C/TIMER0/TIMER1 时钟
 │    ├─ GPIO_Init()：所有电源相关引脚方向与初始电平
 │    └─ I2C_Init()：从机地址 0x15，100kHz
 │
 ├─ core_init()                                  core.c:432
 │    ├─ NVIC_SetPriority(SysTick_IRQn, 3)
 │    ├─ InitFlashAndRegist()                    读/校验/初始化 Flash 配置
 │    ├─ pm_task_init()                          注册 PowerManageTask + 中断
 │    ├─ wol_init()                              按配置决定是否配置 PHY
 │    ├─ update_task_init()                      注册升级任务
 │    ├─ Timer1_Init()                           LED 10ms tick
 │    ├─ init_led_colors()
 │    └─ adc_key_init()
 │
 └─ while(1)                                     main.c:55
      ├─ ScheduleTaskWorkerLooper()              UpdateTask → PowerManageTask
      └─ RGB_Process()                           灯条刷新
```

### 2.2 三个执行上下文

| 上下文 | 机制 | 跑什么 |
|---|---|---|
| 主循环 | `RegistTaskWorker()` 链表 | `UpdateTaskWorker`、`PowerManageTaskWorker`，每圈都跑 |
| SysTick 1ms（优先级 3） | `vtimer_hander()`；每 10 拍调 `TrigerAlarmEventCallback(10)` | `power_control` 状态机每 1ms 推进；`WatchdogAlarmItem`、`AdcKeyAlarmItem` 每 10ms |
| I2C 中断（优先级 0） | 从机地址 0x15 | 写 `[reg][value]` → 回调链；读 → `GetRegistBufferValue()` |
| GPIO 中断（优先级 1 / EINT1 0） | 边沿中断 | 电源键、WOL PME、RTC PME、PWR_GOOD、主机喂狗 |

> `power_control.c` 的 1ms 虚拟定时器是既有体系的一部分，由 `SysTick_Handler()` 每 tick 调 `vtimer_hander()` 驱动。宏名里的 `TIMER1_*` 是历史遗留，实际是软件毫秒值，与硬件 TIMER1 无关。

---

## 3. 双层电源状态机

### 3.1 `pm.c` — `host_status`（策略层，主循环）

`host_status` 初值为 `POWER_LOSS`（`pm.c:21`）。

```text
POWER_LOSS ──HOST_PWR_IN=1──────────────────────> POWER_ON
     │
     ├─ power_loss=1
     │      └─ PowerOnhostBoard() ──────────────> POWER_OFF
     └─ power_loss=0 且 power_on_is_enable_actived=1
            └─ PowerOnhostBoard() ──────────────> POWER_OFF

POWER_OFF ──按键 / RTC / WOL / IR / soc_reboot_flag──> PowerOnhostBoard()
     │
     └─ get_host_power_status()==POWER_ON ──────> POWER_ON

POWER_ON  ──is_host_poweroff()──> RGB_HostOff()
                                   + config_wol()（若 WOL 开启）
                                   ─────────────> POWER_OFF

POWER_RESET ──看门狗超时──> ResetHostBoard():
                             关电（≤10s）→ 1s → 上电（2s）
```

关键动作：

| 函数 | 行为 |
|---|---|
| `PowerOnhostBoard()` `pm.c:233` | 置 `NV_REQ_POWERON`；LED 闪红；`Delay100ms(20)` 阻塞 2s；按配置/恢复键处理看门狗 |
| `PowerDownHostBoard()` `pm.c:263` | 置 `NV_REQ_POWEROFF`；等 `HOST_PWR_IN==0`，最长 10s |
| `ResetHostBoard()` `pm.c:281` | 关电 → 1s → 上电 |

### 3.2 `power_control.c` — `power_event`（执行层，SysTick 1ms）

```text
PowerOff ──NV_REQ_POWERON（或按键释放→按下）──> PoweringOn

PoweringOn:
    HOST_POWER_ENABLE = 1
    if (PWR_GOOD) {
        VIN_PWR_ON = 1
        +80ms  → PMIC_EN0 = 1
        +200ms → if (PWR_GOOD && SoC状态==POWER_ON)
                     RGB_HostOn(); → PowerOn
                 else
                     soc_reboot_flag = 1; → PoweringOff
    }

PowerOn:
    10s 长按 / PWR_GOOD=0 / FORCE_OFF=0 / SoC状态=POWER_OFF / NV_REQ_POWEROFF
        → PoweringOff

PoweringOff:
    RGB_HostOff()
    非 WOL 模式：HOST_POWER_ENABLE = 0
    PMIC_EN0 = 0
    +10ms  → VIN_PWR_ON = 0
    +200ms → PowerOff

PowerOff:
    PMIC_EN0 = 0; VIN_PWR_ON = 0
    清 NV_REQ_POWEROFF
    等 NV_REQ_POWERON 或按键
```

状态机中两个已定义但无人进入的状态：`Reset()` 和 `POWER_EVENT_ERROR`。`power_off_state` 只赋值、无人读取。

### 3.3 两个状态机如何耦合

```text
主循环（pm.c）                              SysTick 1ms（power_control.c）
     │                                              │
 PowerOnhostBoard()                                 │
     ├─ RGB 闪红                                     │
     ├─ pending |= NV_REQ_POWERON ──────────────────>│ PowerOff 看到请求
     ├─ Delay 2s（阻塞）                             ├─ HOST_POWER_ENABLE=1
     │                                              ├─ PWR_GOOD? VIN=1
     │                                              ├─ +80ms  PMIC_EN=1
     │                                              ├─ +200ms 回读
     │                                              └─ PowerOn / RGB_HostOn
     └─ 继续轮询 host_status
```

共享介质：

| 共享介质 | 方向 | 用途 |
|---|---|---|
| `pending_power_request` | pm 置位，power_control 在中断里清位 | `NV_REQ_POWERON` / `NV_REQ_POWEROFF`，唯一显式接口 |
| `HOST_POWER_ENABLE` / `VIN_PWR_ON` / `PMIC_EN0` | power_control 写，双方读 | 结果回读 |
| `P5.3`（PWR_GOOD）、`P3.2/P5.4`（SoC 状态） | 硬件 | 上电/掉电判定 |
| `RegistBuffer.soc_reboot_flag` | 双向 | power_control 上电失败置 1，pm 在 `POWER_OFF` 重试 |
| `get_poweron_times()` | power_control 计数，`core.c` 供主机读 | 调试/统计 |

> `PoweringOn` 的入口要求 **PWR_GOOD 已经为高**（`power_control.c:289`），否则只拉高 `HOST_POWER_ENABLE` 后原地等待，没有超时。

---

## 4. 上电/关电时序

### 4.1 上电

| 阶段 | 动作 | 时间 |
|---|---|---|
| 上电请求 | `PowerOnhostBoard()` 置 `NV_REQ_POWERON`、灯闪红 | 阻塞 2s |
| 12V 使能 | `HOST_POWER_ENABLE = 1` | 0 |
| 核心板电 | `VIN_PWR_ON = 1` | PWR_GOOD 已高时立即 |
| PMIC 电 | `PMIC_EN0 = 1` | +80ms |
| 回读 | PWR_GOOD + SoC 状态 | +200ms |
| 成功 | `RGB_HostOn()`，进 `PowerOn` | — |

### 4.2 关电

| 阶段 | 动作 | 时间 |
|---|---|---|
| 开始 | `RGB_HostOff()` | 0 |
| 12V | 非 WOL 模式下拉低 `HOST_POWER_ENABLE` | 0 |
| PMIC | `PMIC_EN0 = 0` | 0 |
| 断 VIN | `VIN_PWR_ON = 0` | +10ms |
| 放完电 | 进 `PowerOff` | +200ms |

> WOL 开启时 `HOST_POWER_ENABLE` 保持为 1，以维持 12V/PHY 的待机供电；关电前 `pm.c` 会先调用 `config_wol()` 把 PHY 配成 magic packet 唤醒模式。

---

## 5. 唤醒源、关机判定与看门狗

### 5.1 唤醒源

| 唤醒源 | 检测路径 | 条件 |
|---|---|---|
| 电源键 P3.0 | 下降沿 → `power_button_pressed=1` | `host_status==POWER_OFF`；`PwrInDelay==0` 走 100µs 防抖，否则走长按秒数门槛 |
| WOL P0.1 | PHY PME 边沿 → `wol_pme_actived=1` | `host_status==POWER_OFF`、`wol_wake_en=1` |
| RTC P5.2 | EINT1 下降沿 → `rtc_pme_actived=1` | `host_status==POWER_OFF`、`rtc_wake_en=1` |
| 掉电自启 | 启动时 `POWER_LOSS` 态；或 P5.3 下降沿 | flash `power_loss=1`，或 `power_loss=0` 时等 `power_on_is_enable_actived` |
| 重启重试 | `soc_reboot_flag==1` | 上电失败 / 主机 `MCU_PM_CTRL=3` |
| IR | `ir_pme_actived` | 代码存在，但 `ir.c` 未编译，永远不触发 |

### 5.2 关机判定

`is_host_poweroff()`（`pm.c:407`）分两种：

- **WOL 开启**：看 SoC 状态位 `P3.2/P5.4 == POWER_OFF`。因为 12V 和 PHY 还保着，PWR_GOOD 可能不掉。
- **WOL 关闭**：直接看 `HOST_PWR_IN(P5.3) == 0`。

另外 `power_control.c` 自己还会检测：

- 电源键长按 10s；
- `PWR_GOOD == 0`；
- `FORCE_OFF(P0.5) == 0`；
- SoC 状态为 `POWER_OFF`；
- 收到 `NV_REQ_POWEROFF`。

### 5.3 主机看门狗

这是 **给主机用的看门狗**，不是 MCU 自己的 WDT：

- 主机在 P3.6 给下降沿喂狗；
- 超时时间 = `WdtTimout × 6s`（2..100 → 12..600s，默认 2 = 12s）；
- 上电时先给 300s 启动宽限（`PowerOnhostBoard()`）；
- 超时 → `host_status = POWER_RESET` → `ResetHostBoard()`；
- 恢复键 P3.1 在开机时被按住会直接关掉看门狗；
- `MCU_PM_CTRL=3` 会把看门狗闹钟设为 100s。

MCU 自身硬件 WDT 当前未启用。

---

## 6. 主机接口与配置持久化

### 6.1 I2C 从机寄存器

- 从机地址：**0x15**（7bit）；
- 写事务：`[reg][value]` 两字节，经 `TransformBufferCallback()` 分发，第一个 `return 0` 的模块消费；
- 读事务：`GetRegistBufferValue()` 的 `switch(reg)`，一次回 1 字节，>8bit 拆 `_H`/`_L`。

关键寄存器：

| 寄存器 | 名称 | 作用 |
|---|---|---|
| `0x09` | `MCU_POWERIN_DELAY` | 电源键开机长按延时（秒） |
| `0x0D` | `MCU_POWER_LOSS` | 掉电自启开关 |
| `0x0F` | `MCU_WDT_CONTROL` | 主机看门狗开关 |
| `0x30` | `MCU_WDT_TIMOUT` | 看门狗超时（×6s） |
| `0x1E` | `MCU_SATA_PWR` | SATA 电源开关 |
| `0x20` | `MCU_RTC_WAKE` | RTC 唤醒开关 |
| `0x2E` | `MCU_WOL_WAKE` | WOL 唤醒开关 |
| `0x27–0x2C` | `MCU_ETH_MAC1..6` | WOL MAC 地址 |
| `0x23` | `MCU_PM_CTRL` | 主机电源控制：1 关机 / 2 整机复位 / 3 延迟上电 |
| `0x55–0x59` | `MCU_LED_*` | LED 模式、颜色、闪烁周期 |
| `0x61–0x63` | `MCU_ADC_KEY*` | ADC 按键值 / 原始采样 |
| `0x60` | `MCU_UPDATE_CTRL` | 升级并跳 loader |

### 6.2 Flash 配置

`struct FlashRegist`（`core.h:93`）保存在数据 Flash 参数页：

| 字段 | 说明 |
|---|---|
| `Signature` | `0x55AA55AA` 有效标志 |
| `WdtTimout` | 看门狗超时（×6s） |
| `PwrInDelay` | 开机长按延时（秒） |
| `mac[6]` | WOL MAC 地址 |
| `control.power_loss` | 掉电自启 |
| `control.rtc_wake_en` | RTC 唤醒使能 |
| `control.wol_wake_en` | WOL 唤醒使能 |
| `control.watchdog_en` | 主机看门狗使能 |
| `control.WdtTimout` / `WdtStartDelay` | 看门狗位域（历史字段） |
| `color[3]` / `blinkTimer` | LED 颜色 / 闪烁周期 |

启动时 `InitFlashAndRegist()`（`core.c:404`）校验签名；非法则写默认值。任何配置写入都会调用 `FlashRegistUpdate()`（`core.c:360`）擦写整个 512B 页。

默认值（`core.c:375`）：

- `power_loss = 0`
- `rtc_wake_en = 1`
- `wol_wake_en = 1`
- `watchdog_en = 0`
- `WdtTimout = 2`
- `PwrInDelay = 0`
- MAC = `A6:0B:0C:3F:12:F4`（示例值）
- LED 颜色 = `0xFF0000`，闪烁周期 = 5

---

## 7. 配套模块

### 7.1 WOL（`wol.c`）

- MDIO 位操作：P4.6 = MDC，P4.7 = MDIO；
- 支持 PHY：SMSC8742、RTL8201F、RTL8211E、RTL8211F、**RTL8261C（本板）**、JL1111、JL2121、YT8521S、YT8531S；
- `config_wol()`（`wol.c:1004`）：
  1. `HOST_WOL_EN_OUT` 拉高 200ms 再拉低；
  2. 等 3s；
  3. MDIO 读 PHY ID；
  4. 写 MAC 和 magic packet 唤醒使能；
- 主机掉电时 `pm.c` 先调 `config_wol()`，再使能 P0.1 中断；
- 唤醒中断沿由 `wol_int_flag()` 决定：JL1111 用上升沿，其余用下降沿；
- 板级实测：RTL8261C，MDIO 地址 0，`0xD8A0` 写 `0x1000`。

### 7.2 LED（`led.c`）

- 数据脚 P2.2，软件 bit-bang GRB，24bit × 4 颗；
- 时基：Timer1 10ms 中断累加 `tick_100ms`；
- 模式：常亮（0）/ 闪烁（1）/ 呼吸（2）；
- `led_host_off` 锁存：主机掉电时 `RGB_HostOff()` 立即灭灯，运行期不会自动复亮；
- 上电过程中 `PowerOnhostBoard()` 设红灯闪烁，上电成功后 `RGB_HostOn()` 恢复显示。

### 7.3 SATA 电源（`pm.c`）

- 引脚 P2.4，低有效；
- 主机通过 `MCU_SATA_PWR(0x1E)` 写 0/1；
- `POWER_LOSS` / `POWER_OFF` 态会强制关闭 SATA 电源；
- 默认关闭。

### 7.4 ADC 按键（`adc_key.c`）

- 引脚 P1.5 / AIN5；
- 10ms 扫描一次，连续 3 次一致才确认（30ms 防抖）；
- 三键电阻梯窗口同时兼容两种板子的分压比；
- 主机通过 `MCU_ADC_KEY(0x61)` 读键值，`0x62/0x63` 读原始采样。

### 7.5 固件升级（`update.c`）

- 主机写 `MCU_UPDATE_CTRL(0x60) = 1`；
- `RebootUpdateLoader()` 清除 `Signature` 的固件有效位；
- `UpdateTaskWorker()` 调 `RebootToLoader()`：关 WDT → `FMC_SET_LDROM_BOOT()` → CPU 复位；
- loader 位于 LDROM（同级仓库 `../mini54-loader`）。

### 7.6 恢复键

- P3.1 `HOST_RECOVERY_IN`，低有效；
- 上电时按住会关闭主机看门狗，便于恢复。

---

## 8. 关键引脚速查

| 功能 | 引脚 |
|---|---|
| 电源键输入 | P3.0 |
| 主机喂狗输入 | P3.6 |
| 恢复键输入 | P3.1 |
| PWR_GOOD / S5_PWRGD | P5.3 |
| SoC 电源状态 BIT0 / BIT1 | P3.2 / P5.4 |
| 12V 使能 `HOST_POWER_ENABLE` | P2.3 |
| 核心板 `VIN_PWR_ON` | P0.0 |
| PMIC 使能 `NV_POWER_EN` | P0.6 |
| 强制关机检测 `FORCE_OFF` | P0.5 |
| SATA 电源（低有效） | P2.4 |
| WOL 使能输出 | P0.4 |
| WOL PME 输入 | P0.1 |
| RTC PME 输入 | P5.2 |
| RGB 数据 | P2.2 |
| ADC 按键 | P1.5 / AIN5 |
| MDC / MDIO | P4.6 / P4.7 |
| 主机电源键输出（当前未驱动） | P2.5 |
| SoC 复位 / 系统复位输出（当前未驱动） | P1.4 / P2.6 |

---

## 9. 设计特点与注意事项

1. **主循环会被阻塞**
   - `PowerOnhostBoard()`：`Delay100ms(20)` = 2s；
   - `PowerDownHostBoard()`：最长 10s；
   - `config_wol()`：约 3.2s。

   阻塞期间 LED 刷新和 `UpdateTask` 停摆，但 `power_control` 在 SysTick 中断里继续推进，电源时序不受影响。

2. **Flash 写在 I2C 中断里**
   主机每改一个配置字节就擦写一页，耗时数毫秒，会阻塞 I2C。

3. **`PoweringOn` 入口要求 PWR_GOOD 已高**
   `power_control.c:289` 先检查 `PWR_GOOD` 才启动 VIN/PMIC 时序；不满足时只拉高 `HOST_POWER_ENABLE` 原地等待，没有超时。

4. **`check_pwr_btn_action_time()` 超过 4s 返回 0**
   即长按太久反而失败；`PwrInDelay>0` 时实际是一个 0～4s 的窗口，而不是“至少 N 秒”。

5. **`soc_reboot_flag` 重试没有上限**
   当前代码里上电失败会反复重试。

6. **Flash 地址与容量**
   `core.h` 写 `DATA_FLASH_BASE = 0x3E00`，但芯片实测 `DFBADR = 0x3C00`，APROM 实际只有 15KB；scatter 允许链接到 `0x3E00` 且不报警，越界会擦掉自己的代码。

7. **未编译 / 死代码**
   - `ir.c`、`function/src/watchdog.c` 不在 Keil 工程；
   - `board.c` 的 `WDT_Init()` 被注释；
   - `power_control.c` 的 `Reset()` 状态从未进入；
   - `P2.5 / P1.4 / P2.6` 目前只初始化未驱动。

8. **两套状态机不是严格镜像**
   `pm.c` 的 `host_status` 与 `power_control.c` 的 `power_event` 各自推进，只通过请求位、GPIO 和 `RegistBuffer` 同步；读代码时不要把两者当成一个状态机。

---

## 10. 关键代码位置索引

| 内容 | 位置 |
|---|---|
| 主循环 | `main.c:55` |
| `core_init()` | `core/core.c:432` |
| SysTick 中断 | `core/core.c:109` |
| 任务调度 | `core/core.c:148` |
| 10ms 闹钟 | `core/core.c:163` |
| I2C 写回调链 | `core/core.c:182` |
| 主机读寄存器 | `core/core.c:200` |
| Flash 配置读写 | `core/core.c:360`、`core/core.c:404` |
| `pm` 状态机 | `function/src/pm.c:291` |
| `PowerOnhostBoard()` | `function/src/pm.c:233` |
| `PowerDownHostBoard()` | `function/src/pm.c:263` |
| `ResetHostBoard()` | `function/src/pm.c:281` |
| 主机看门狗超时 | `function/src/pm.c:389` |
| 电源键 / 喂狗中断 | `function/src/pm.c:440` |
| WOL / RTC / PWR_GOOD 中断 | `function/src/pm.c:423`、`pm.c:469` |
| `power_control` 状态机 | `function/src/power_control.c:228` |
| `PowerOff` / `PoweringOn` / `PowerOn` / `PoweringOff` | `power_control.c:242 / 277 / 327 / 385` |
| 虚拟定时器 | `function/src/power_control.c:199` |
| `get_host_power_status()` | `board/board.c:130` |
| I2C 从机初始化 | `board/board.c:146` |
| WOL 配置 | `function/src/wol.c:1004` |
| WOL 初始化 | `function/src/wol.c:1033` |
| LED 刷新 | `function/src/led.c:245` |
| LED 灭/亮 | `function/src/led.c:224` |
| ADC 按键扫描 | `function/src/adc_key.c:160` |
| 升级跳转 | `function/src/update.c:40` |
