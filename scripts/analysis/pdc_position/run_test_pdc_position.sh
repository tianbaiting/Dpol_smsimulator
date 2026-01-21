#bin/bash
# PDC位置计算测试运行脚本
        # {0.8, "141114-0,8T-3000.table"},
        # {1.0, "180626-1,00T-3000.table"},
        # {1.2, "180626-1,20T-3000.table"},
        # {1.4, "180703-1,40T-3000.table"},
        # {1.6, "180626-1,60T-3000.table"},
        # {1.8, "180703-1,80T-3000.table"},
        # {2.0, "180627-2,00T-3000.table"},
        # {2.2, "180628-2,20T-3000.table"},
        # {2.4, "180702-2,40T-3000.table"},
        # {2.6, "180703-2,60T-3000.table"},
        # {2.8, "180628-2,80T-3000.table"},
        # {3.0, "180629-3,00T-3000.table"}
 
# 获取环境变量

# 批量运行3000.table
# 测试 0.8T 磁场
$SMSIMDIR/build/bin/test_pdc_position $SMSIMDIR/configs/simulation/geometry/filed_map/141114-0,8T-3000.table

# 测试 1.0T 磁场（默认）
$SMSIMDIR/build/bin/test_pdc_position $SMSIMDIR/configs/simulation/geometry/filed_map/180626-1,00T-3000.table

# 测试 1.2T 磁场
$SMSIMDIR/build/bin/test_pdc_position $SMSIMDIR/configs/simulation/geometry/filed_map/180626-1,20T-3000.table

# 测试 1.4T 磁场
$SMSIMDIR/build/bin/test_pdc_position $SMSIMDIR/configs/simulation/geometry/filed_map/180703-1,40T-3000.table

# 测试 1.6T 磁场
$SMSIMDIR/build/bin/test_pdc_position $SMSIMDIR/configs/simulation/geometry/filed_map/180626-1,60T-3000.table
# 测试 1.8T 磁场
$SMSIMDIR/build/bin/test_pdc_position $SMSIMDIR/configs/simulation/geometry/filed_map/180703-1,80T-3000.table
# 测试 2.0T 磁场
$SMSIMDIR/build/bin/test_pdc_position $SMSIMDIR/configs/simulation/geometry/filed_map/180627-2,00T-3000.table
# 测试 2.2T 磁场
$SMSIMDIR/build/bin/test_pdc_position $SMSIMDIR/configs/simulation/geometry/filed_map/180628-2,20T-3000.table
# 测试 2.4T 磁场
$SMSIMDIR/build/bin/test_pdc_position $SMSIMDIR/configs/simulation/geometry/filed_map/180702-2,40T-3000.table
# 测试 2.6T 磁场
$SMSIMDIR/build/bin/test_pdc_position $SMSIMDIR/configs/simulation/geometry/filed_map/180703-2,60T-3000.table
# 测试 2.8T 磁场
$SMSIMDIR/build/bin/test_pdc_position $SMSIMDIR/configs/simulation/geometry/filed_map/180628-2,80T-3000.table
# 测试 3.0T 磁场
$SMSIMDIR/build/bin/test_pdc_position $SMSIMDIR/configs/simulation/geometry/filed_map/180629-3,00T-3000.table
