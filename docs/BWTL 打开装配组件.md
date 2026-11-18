# BWTL 打开装配组件

在 Creo 装配树上选中一个子零件或子装配，在 BWTL 聊天里发送「打开选中的组件」。也可以发送「打开子零件 M26A0181001V00.PRT」「打开子装配 FRAME.ASM」或「打开装配组件 ID:43/42」。指定实例路径时，使用这次装配树返回的真实 ID。父装配和钣金子件同名时，按模型类型检查。相同名称对应多个实例时，请选实例或指定路径。

BWTL 从当前装配树解析活动组件。支持嵌套的子装配和子零件，也能切换到已打开的同一模型窗口。组件被抑制、简化表示排除或尚未加载时，先在 Creo 恢复该组件或切换至包含它的表示。打开操作保留父装配窗口、工作目录、文件版本和未保存修改。可通过 Creo 的窗口列表切回父装配。相同零件的重复实例仍引用同一个 Creo 模型，后续修改该零件会反映到引用它的实例。

原生协议为 `CREO_OP ASM_OPEN <当前装配名> <实例ID、唯一名称或 SELECTED>`，当前装配下旧的 DISPLAY 也按实际装配树解析。BWTL 调用原生窗口激活后，把控制权交回 Creo，再核对实际活动窗口和目标模型。见 [PTC 窗口激活接口](https://support.ptc.com/help/creo_toolkit/protoolkit_pma/r12/chinese_tw/creo_toolkit/Activating_Windows.html)。

安装 `0.23.0-local.componentopen.1` 后，正常重启 Creo。在独立 Creo 会话验证同名钣金子件、嵌套子装配与子零件、重复实例、树中单选以及错误或多义输入。需要回退时恢复 `0.23.0-local.companydesign.1`，再正常重启 Creo。
