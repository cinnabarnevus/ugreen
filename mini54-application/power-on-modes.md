# Mini54 四种开机方式对比

> 项目:`/mnt/data/MCU/UBTECH/mini54-application`
> 芯片:Mini54(Newbit/Mini51 系列)MCU,作为主机(如 NVIDIA Jetson)的电源控制器。
> 相关源码:`function/src/pm.c`(电源状态机)、`function/src/wol.c`(PHY 唤醒配置)、`function/src/power_control.c`(硬件上电时序)。

## 一句话概括

四种方式最终都汇聚到 `PowerOnhostBoard()`(pm.c:170),它通过 `pending_power_request |= NV_REQ_POWERON` + `HOST_POWER_ENABLE` 拉高把主机主板拉起来。区别只在于**MCU 是怎么被唤醒、由谁发起**。

| 开机组 | 触发源 | 检测路径 | 需要满足的条件 | 应用场景 |
|---|---|---|---|---|
| **电源键开机** | 本地物理按键 P30 | `GPIO234_IRQHandler` 下降沿 → `power_button_pressed=1` → PM 任务 `POWER_OFF` 态处理 | 无外部依赖 | 人工现场开机 |
| **WOL 网络唤醒** | 网卡 PHY 收到 magic packet,拉高 P0.1 | `GPIO01_IRQHandler` → `wol_pme_actived=1` → PM 任务,并把 `pm_event = EVENT_WOL_WAKEUP` | 网线已连、PHY 保持供电、MAC 已配好、主机支持网卡 WOL | 远程开机 |
| **上电掉电自动重启** | 上电时 `HOST_POWER_IS_ENABLE`(P5.3)/ 掉电恢复位 `power_loss` | 上电进 `POWER_LOSS` 态判断;或 `GPIO5_IRQHandler` 置 `power_on_is_enable_actived=1` | flash 配置 `power_loss` 或主机使能信号 | 掉电自动恢复 |
| **RTC 唤醒** | 主机 RTC 闹钟输出拉高 P5.2 | `EINT1_IRQHandler` → `rtc_pme_actived=1` → PM 任务 | RTC 闹钟预置、RTC 电路保电 | 定时定点开机 |

## 各自的关键代码

### 电源键

在 `POWER_OFF` 态轮询 `power_button_pressed`(pm.c:251);中断入口在 `GPIO234_IRQHandler`(pm.c:375)。按 `PwrInDelay` 配置选择防抖方式(pm.c:254-263):

- `PwrInDelay == 0` → 短按 `pwr_btn_Anti_shake()`
- 否则 → 长按 `check_pwr_btn_action_time() >= PwrInDelay * 1000`

### WOL

关键在 pm.c:293-299:主机要关机时,若 `wol_wake_en == 1`,先配置 PHY 为 magic packet 唤醒(`config_wol()`,wol.c:1004)再 `ENABLED_WOL_INTERRUPT`。收到包后由 `GPIO01_IRQHandler` 置位。

中断沿由 `wol_int_flag()` 决定(pm.h:30-33):JL1111 用上升沿,其余用下降沿。

### 掉电自动重启

宿主 pm.c:229-244。上电进 `POWER_LOSS`:

- `HOST_PWR_IN == 1` → 已是开机态,进 `POWER_ON`;
- `power_loss == 1`(flash 默认为 1,core.c:355)→ 直接 `PowerOnhostBoard()`,即"来电自动恢复"(真正意义的"上电自动开机");
- `power_loss == 0` → 等 `power_on_is_enable_actived`(由 P5.3 `HOST_POWER_IS_ENABLE` 的 `GPIO5_IRQHandler` 置位,pm.c:404)。

### RTC

前提是主机关机前请求 MCU 把 `power_loss`、`rtc_wake_en` 都置 1(core.c:355-356),这样掉电时走 PMIC 保电、RTC 还能走。触发在 pm.c:265,`EINT1_IRQHandler`(pm.c:421)置 `rtc_pme_actived`。

## 主要权衡

- **电源键**:最简单可靠、不依赖任何外部条件,但必须人在现场。
- **WOL**:能远程唤醒,但链路要求最高——要网线、要 PHY 常供电、要能连到主机网口,且 host 上电后才能上报;常作为"低功耗待机 + 网络唤醒"组合用。
- **掉电自动重启**:无人工、故障后自愈,但代价是"来电即开机",对意外掉电/反复上电敏感,所以才有 `power_loss` 开关位控制是否启用。
- **RTC**:最省电的定时唤醒,但与真实时钟绑定,要求主机侧配置闹钟,且 RTC 掉电即失效。

## 需要留意的点

- **电源键**和 **WOL** 在 `POWER_OFF` 态条件下才有效(靠 pm.c:380/365 里 `host_status == POWER_OFF` 判断)。
- **掉电自动重启**用的是上电初始的 `POWER_LOSS` 态。

它们天然互补:**电源键 / WOL / RTC** 管的都是"关机之后如何再启动",而**掉电自动重启**管的是"上电时刻的自恢复"。
