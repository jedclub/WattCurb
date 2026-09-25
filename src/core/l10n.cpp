#include "core/l10n.hpp"
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <cctype>

namespace wattcurb::core::l10n {

namespace {

static std::atomic<Language> g_active_lang{Language::EN};

// [REF-REQ-076, REF-ARCH-053]
// 14 Languages x 47 Strings Matrix aligned to 64 bytes for L1 Data Cache locality
// Languages order: EN, ZH, HI, ES, FR, AR, BN, PT, RU, UR, ID, DE, KO, JA
alignas(64) static constexpr const char* const STRING_TABLE[static_cast<size_t>(Language::COUNT)][static_cast<size_t>(StringId::COUNT)] = {
    // 0: EN (English)
    {
        "Discharging",                                  // STATUS_DISCHARGING
        "AC Passthrough (Full)",                        // STATUS_AC_PASSTHROUGH
        "AC Charging",                                  // STATUS_AC_CHARGING
        "AC Connected",                                 // STATUS_AC_CONNECTED
        "On Battery",                                   // STATUS_ON_BATTERY
        "%u min left",                                  // BATTERY_TIME_LEFT
        "Calculating...",                               // BATTERY_TIME_CALCULATING
        "AC Connected (Unlimited)",                     // BATTERY_TIME_UNLIMITED
        "%1h %2m",                                      // BATTERY_TIME_HOURS_MINS
        "%1m",                                          // BATTERY_TIME_MINS
        "Performance (4.1G Boost)",                     // PROFILE_PERFORMANCE_SHORT
        "Balanced",                                     // PROFILE_BALANCED_SHORT
        "SmartSave (1.7G)",                             // PROFILE_SMARTSAVE_SHORT
        "UltraSave (1.4G)",                             // PROFILE_ULTRASAVE_SHORT
        "Performance (Boost Mode - 4.1GHz)",            // PROFILE_PERFORMANCE_LONG
        "Balanced (Default Recommended)",               // PROFILE_BALANCED_LONG
        "Smart Save (Power Saver - 1.7GHz)",            // PROFILE_SMARTSAVE_LONG
        "Ultra Save (Ultra Low - 1.4GHz Cap)",          // PROFILE_ULTRASAVE_LONG
        "Idle Stable (No Leaks)",                       // HUD_IDLE_STABLE
        "📈 Open Matrix Dashboard",                     // ACTION_OPEN_DASHBOARD
        "🔋 Open Battery Drain Audit Report",            // ACTION_OPEN_BATTERY_REPORT
        "📊 Open KDE System Monitor",                   // ACTION_OPEN_SYSMONITOR
        "CPU Package",                                  // DEV_CPU_PKG
        "GPU Silicon",                                  // DEV_GPU_SILICON
        "Display & Backlight",                          // DEV_DISPLAY
        "Storage & NVMe SSD",                           // DEV_STORAGE
        "Cooling Fan",                                  // DEV_COOLING_FAN
        "Platform / Motherboard",                       // DEV_PLATFORM
        "WattCurb Resident Daemon Binary Status",       // CLI_STATUS_HEADER
        "Total System Drain",                           // CLI_TOTAL_DRAIN
        "CPU Package Drain",                            // CLI_CPU_DRAIN
        "GPU Silicon Drain",                            // CLI_GPU_DRAIN
        "Battery Level",                                // CLI_BATTERY_LEVEL
        "System Wakeups",                               // CLI_WAKEUPS
        "Cooling Fan",                                  // CLI_COOLING_FAN
        "Active Mitigations",                           // CLI_ACTIVE_MITIGATIONS
        "Top Drain Culprit",                            // CLI_TOP_CULPRIT
        "TOTAL DRAIN",                                  // DASH_TOTAL_DRAIN
        "CPU & MEMORY SUBSYSTEM",                       // DASH_CPU_MEM_SUBSYSTEM
        "BATTERY & POWER SUPPLY",                       // DASH_BATTERY_POWER_SUPPLY
        "GPU SILICON & LOAD",                           // DASH_GPU_SILICON_LOAD
        "DISPLAY & BACKLIGHT",                          // DASH_DISPLAY_BACKLIGHT
        "STORAGE & NVME SSD",                           // DASH_STORAGE_NVME
        "HARDWARE POWER SHARES",                        // DASH_POWER_SHARE_HW
        "PROCESS ENERGY CONSUMPTION",                   // DASH_POWER_SHARE_PROC
        "POWER DRAIN TIMELINE",                         // DASH_TIMELINE
        "Details",                                      // DASH_DETAILS
    },
    // 1: ZH (Mandarin Chinese - 简体中文)
    {
        "放电中",                                        // STATUS_DISCHARGING
        "交流电直通 (满电)",                              // STATUS_AC_PASSTHROUGH
        "交流电充电中",                                  // STATUS_AC_CHARGING
        "已连接交流电",                                  // STATUS_AC_CONNECTED
        "电池供电中",                                    // STATUS_ON_BATTERY
        "剩余 %u 分钟",                                  // BATTERY_TIME_LEFT
        "计算中...",                                     // BATTERY_TIME_CALCULATING
        "已接电源 (无限制)",                             // BATTERY_TIME_UNLIMITED
        "%1小时 %2分",                                   // BATTERY_TIME_HOURS_MINS
        "%1分",                                          // BATTERY_TIME_MINS
        "高性能 (4.1G 睿频)",                            // PROFILE_PERFORMANCE_SHORT
        "均衡模式",                                      // PROFILE_BALANCED_SHORT
        "智能节电 (1.7G)",                               // PROFILE_SMARTSAVE_SHORT
        "超极省电 (1.4G)",                               // PROFILE_ULTRASAVE_SHORT
        "高性能模式 (4.1GHz 睿频)",                      // PROFILE_PERFORMANCE_LONG
        "均衡模式 (默认推荐)",                           // PROFILE_BALANCED_LONG
        "智能节电模式 (1.7GHz)",                         // PROFILE_SMARTSAVE_LONG
        "超极省电模式 (1.4GHz 上限)",                    // PROFILE_ULTRASAVE_LONG
        "空闲稳定 (无泄漏)",                             // HUD_IDLE_STABLE
        "📈 打开精确分析矩阵窗口",                       // ACTION_OPEN_DASHBOARD
        "🔋 打开电池深度分析报告",                       // ACTION_OPEN_BATTERY_REPORT
        "📊 打开 KDE 系统监视器",                        // ACTION_OPEN_SYSMONITOR
        "CPU 封装",                                      // DEV_CPU_PKG
        "GPU 核心",                                      // DEV_GPU_SILICON
        "显示器与背光",                                  // DEV_DISPLAY
        "存储与 NVMe SSD",                               // DEV_STORAGE
        "散热风扇",                                      // DEV_COOLING_FAN
        "平台与主板",                                    // DEV_PLATFORM
        "WattCurb 常驻守护进程运行状态",                 // CLI_STATUS_HEADER
        "系统总功耗",                                    // CLI_TOTAL_DRAIN
        "CPU 封装功耗",                                  // CLI_CPU_DRAIN
        "GPU 核心功耗",                                  // CLI_GPU_DRAIN
        "电池电量",                                      // CLI_BATTERY_LEVEL
        "系统唤醒次数",                                  // CLI_WAKEUPS
        "散热风扇转速",                                  // CLI_COOLING_FAN
        "已激活节能策略",                                // CLI_ACTIVE_MITIGATIONS
        "主要耗电进程",                                  // CLI_TOP_CULPRIT
        "总功耗",                                        // DASH_TOTAL_DRAIN
        "CPU 与内存子系统",                              // DASH_CPU_MEM_SUBSYSTEM
        "电池与电源供应",                                // DASH_BATTERY_POWER_SUPPLY
        "GPU 芯片与负载",                                // DASH_GPU_SILICON_LOAD
        "显示器与背光",                                  // DASH_DISPLAY_BACKLIGHT
        "存储与 NVMe SSD",                               // DASH_STORAGE_NVME
        "硬件功耗占比",                                  // DASH_POWER_SHARE_HW
        "进程耗电排行",                                  // DASH_POWER_SHARE_PROC
        "功耗历史曲线",                                  // DASH_TIMELINE
        "详情",                                          // DASH_DETAILS
    },
    // 2: HI (Hindi - हिन्दी)
    {
        "डिस्चार्ज हो रहा है",                            // STATUS_DISCHARGING
        "AC सीधा जुड़ा (पूर्ण चार्ज)",                   // STATUS_AC_PASSTHROUGH
        "AC चार्ज हो रहा है",                            // STATUS_AC_CHARGING
        "AC जुड़ा हुआ है",                               // STATUS_AC_CONNECTED
        "बैटरी पर",                                      // STATUS_ON_BATTERY
        "%u मिनट शेष",                                   // BATTERY_TIME_LEFT
        "गणना हो रही है...",                             // BATTERY_TIME_CALCULATING
        "पावर कनेक्टेड (असीमित)",                        // BATTERY_TIME_UNLIMITED
        "%1 घंटे %2 मिनट",                               // BATTERY_TIME_HOURS_MINS
        "%1 मिनट",                                       // BATTERY_TIME_MINS
        "प्रदर्शन (4.1G बूस्ट)",                         // PROFILE_PERFORMANCE_SHORT
        "संतुलित",                                       // PROFILE_BALANCED_SHORT
        "स्मार्ट सेव (1.7G)",                            // PROFILE_SMARTSAVE_SHORT
        "अल्ट्रा सेव (1.4G)",                            // PROFILE_ULTRASAVE_SHORT
        "उच्च प्रदर्शन मोड (4.1GHz बूस्ट)",               // PROFILE_PERFORMANCE_LONG
        "संतुलित मोड (अनुशंसित)",                        // PROFILE_BALANCED_LONG
        "स्मार्ट सेव मोड (1.7GHz)",                      // PROFILE_SMARTSAVE_LONG
        "अल्ट्रा सेव मोड (1.4GHz सीमा)",                 // PROFILE_ULTRASAVE_LONG
        "निष्क्रिय स्थिर (कोई रिसाव नहीं)",               // HUD_IDLE_STABLE
        "📈 मैट्रिक्स डैशबोर्ड खोलें",                  // ACTION_OPEN_DASHBOARD
        "🔋 बैटरी ड्रेन ऑडिट रिपोर्ट खोलें",              // ACTION_OPEN_BATTERY_REPORT
        "📊 KDE सिस्टम मॉनिटर खोलें",                    // ACTION_OPEN_SYSMONITOR
        "CPU पैकेज",                                     // DEV_CPU_PKG
        "GPU सिलिकॉन",                                   // DEV_GPU_SILICON
        "डिस्प्ले और बैकलाइट",                           // DEV_DISPLAY
        "स्टोरेज और NVMe SSD",                            // DEV_STORAGE
        "कूलिंग फैन",                                    // DEV_COOLING_FAN
        "प्लेटफ़ॉर्म / मदरबोर्ड",                        // DEV_PLATFORM
        "WattCurb डेमन बाइनरी स्थिति",                   // CLI_STATUS_HEADER
        "कुल सिस्टम पावर खपत",                           // CLI_TOTAL_DRAIN
        "CPU पैकेज पावर",                                // CLI_CPU_DRAIN
        "GPU सिलिकॉन पावर",                              // CLI_GPU_DRAIN
        "बैटरी स्तर",                                    // CLI_BATTERY_LEVEL
        "सिस्टम वेकअप",                                  // CLI_WAKEUPS
        "कूलिंग फैन गति",                                // CLI_COOLING_FAN
        "सक्रिय शमन",                                    // CLI_ACTIVE_MITIGATIONS
        "शीर्ष पावर उपभोक्ता",                           // CLI_TOP_CULPRIT
        "कुल खपत",                                       // DASH_TOTAL_DRAIN
        "CPU और मेमोरी सब-सिस्टम",                        // DASH_CPU_MEM_SUBSYSTEM
        "बैटरी और पावर सप्लाई",                           // DASH_BATTERY_POWER_SUPPLY
        "GPU सिलिकॉन और लोड",                            // DASH_GPU_SILICON_LOAD
        "डिस्प्ले और बैकलाइट",                           // DASH_DISPLAY_BACKLIGHT
        "स्टोरेज और NVMe SSD",                            // DASH_STORAGE_NVME
        "हार्डवेयर पावर शेयर",                           // DASH_POWER_SHARE_HW
        "प्रोसेस पावर खपत",                              // DASH_POWER_SHARE_PROC
        "पावर ड्रेन टाइमलाइन",                           // DASH_TIMELINE
        "विवरण",                                         // DASH_DETAILS
    },
    // 3: ES (Spanish - Español)
    {
        "Descargando",                                   // STATUS_DISCHARGING
        "Red directa (Carga completa)",                  // STATUS_AC_PASSTHROUGH
        "Cargando en red",                               // STATUS_AC_CHARGING
        "Conectado a la red",                            // STATUS_AC_CONNECTED
        "Usando batería",                                // STATUS_ON_BATTERY
        "%u min restantes",                              // BATTERY_TIME_LEFT
        "Calculando...",                                 // BATTERY_TIME_CALCULATING
        "Conectado (Ilimitado)",                         // BATTERY_TIME_UNLIMITED
        "%1h %2m",                                       // BATTERY_TIME_HOURS_MINS
        "%1m",                                           // BATTERY_TIME_MINS
        "Rendimiento (Boost 4.1G)",                      // PROFILE_PERFORMANCE_SHORT
        "Equilibrado",                                   // PROFILE_BALANCED_SHORT
        "Ahorro Inteligente (1.7G)",                     // PROFILE_SMARTSAVE_SHORT
        "Ultra Ahorro (1.4G)",                           // PROFILE_ULTRASAVE_SHORT
        "Modo Rendimiento (Boost 4.1GHz)",              // PROFILE_PERFORMANCE_LONG
        "Modo Equilibrado (Recomendado)",               // PROFILE_BALANCED_LONG
        "Modo Ahorro Inteligente (1.7GHz)",             // PROFILE_SMARTSAVE_LONG
        "Modo Ultra Ahorro (Límite 1.4GHz)",            // PROFILE_ULTRASAVE_LONG
        "Inactivo estable (Sin fugas)",                 // HUD_IDLE_STABLE
        "📈 Abrir panel de control Matrix",             // ACTION_OPEN_DASHBOARD
        "🔋 Abrir informe de auditoría de batería",       // ACTION_OPEN_BATTERY_REPORT
        "📊 Abrir Monitor del Sistema KDE",             // ACTION_OPEN_SYSMONITOR
        "Paquete CPU",                                  // DEV_CPU_PKG
        "Silicio GPU",                                  // DEV_GPU_SILICON
        "Pantalla y retroiluminación",                  // DEV_DISPLAY
        "Almacenamiento y SSD NVMe",                    // DEV_STORAGE
        "Ventilador de refrigeración",                  // DEV_COOLING_FAN
        "Plataforma / Placa base",                      // DEV_PLATFORM
        "Estado del demonio residente WattCurb",        // CLI_STATUS_HEADER
        "Consumo total del sistema",                    // CLI_TOTAL_DRAIN
        "Consumo del paquete CPU",                      // CLI_CPU_DRAIN
        "Consumo de GPU",                               // CLI_GPU_DRAIN
        "Nivel de batería",                             // CLI_BATTERY_LEVEL
        "Despertares del sistema",                      // CLI_WAKEUPS
        "Ventilador",                                   // CLI_COOLING_FAN
        "Mitigaciones activas",                         // CLI_ACTIVE_MITIGATIONS
        "Proceso de mayor consumo",                     // CLI_TOP_CULPRIT
        "CONSUMO TOTAL",                                // DASH_TOTAL_DRAIN
        "SUBSISTEMA CPU Y MEMORIA",                     // DASH_CPU_MEM_SUBSYSTEM
        "BATERÍA Y ALIMENTACIÓN",                       // DASH_BATTERY_POWER_SUPPLY
        "SILICIO GPU Y CARGA",                          // DASH_GPU_SILICON_LOAD
        "PANTALLA Y RETROILUMINACIÓN",                  // DASH_DISPLAY_BACKLIGHT
        "ALMACENAMIENTO Y SSD NVME",                    // DASH_STORAGE_NVME
        "DISTRIBUCIÓN DE ENERGÍA DE HARDWARE",          // DASH_POWER_SHARE_HW
        "CONSUMO POR PROCESSOS",                        // DASH_POWER_SHARE_PROC
        "LÍNEA TEMPORAL DE ENERGÍA",                    // DASH_TIMELINE
        "Detalles",                                     // DASH_DETAILS
    },
    // 4: FR (French - Français)
    {
        "En décharge",                                  // STATUS_DISCHARGING
        "Secteur direct (Pleine charge)",               // STATUS_AC_PASSTHROUGH
        "En charge secteur",                            // STATUS_AC_CHARGING
        "Secteur branché",                              // STATUS_AC_CONNECTED
        "Sur batterie",                                 // STATUS_ON_BATTERY
        "%u min restantes",                             // BATTERY_TIME_LEFT
        "Calcul en cours...",                           // BATTERY_TIME_CALCULATING
        "Sur secteur (Illimité)",                       // BATTERY_TIME_UNLIMITED
        "%1h %2m",                                      // BATTERY_TIME_HOURS_MINS
        "%1m",                                          // BATTERY_TIME_MINS
        "Performance (4.1G Boost)",                     // PROFILE_PERFORMANCE_SHORT
        "Équilibré",                                    // PROFILE_BALANCED_SHORT
        "Éco Intelligent (1.7G)",                       // PROFILE_SMARTSAVE_SHORT
        "Ultra Économie (1.4G)",                        // PROFILE_ULTRASAVE_SHORT
        "Mode Performance (Boost 4.1GHz)",              // PROFILE_PERFORMANCE_LONG
        "Mode Équilibré (Recommandé)",                  // PROFILE_BALANCED_LONG
        "Mode Éco Intelligent (1.7GHz)",                // PROFILE_SMARTSAVE_LONG
        "Mode Ultra Économie (Limite 1.4GHz)",          // PROFILE_ULTRASAVE_LONG
        "Veille stable (Aucune fuite)",                 // HUD_IDLE_STABLE
        "📈 Ouvrir le tableau de bord Matrix",          // ACTION_OPEN_DASHBOARD
        "🔋 Ouvrir le rapport d'audit de batterie",       // ACTION_OPEN_BATTERY_REPORT
        "📊 Ouvrir le moniteur système KDE",            // ACTION_OPEN_SYSMONITOR
        "Package CPU",                                  // DEV_CPU_PKG
        "Silicium GPU",                                 // DEV_GPU_SILICON
        "Écran et rétroéclairage",                      // DEV_DISPLAY
        "Stockage et SSD NVMe",                         // DEV_STORAGE
        "Ventilateur",                                  // DEV_COOLING_FAN
        "Plateforme / Carte mère",                      // DEV_PLATFORM
        "État du démon résident WattCurb",              // CLI_STATUS_HEADER
        "Consommation totale du système",               // CLI_TOTAL_DRAIN
        "Consommation Package CPU",                     // CLI_CPU_DRAIN
        "Consommation GPU",                             // CLI_GPU_DRAIN
        "Niveau de batterie",                           // CLI_BATTERY_LEVEL
        "Réveils système",                              // CLI_WAKEUPS
        "Ventilateur",                                  // CLI_COOLING_FAN
        "Atténuations actives",                         // CLI_ACTIVE_MITIGATIONS
        "Processus le plus énergivore",                 // CLI_TOP_CULPRIT
        "CONSOMMATION TOTALE",                          // DASH_TOTAL_DRAIN
        "SOUS-SYSTÈME CPU ET MÉMOIRE",                  // DASH_CPU_MEM_SUBSYSTEM
        "BATTERIE ET ALIMENTATION",                     // DASH_BATTERY_POWER_SUPPLY
        "SILICIUM GPU ET CHARGE",                       // DASH_GPU_SILICON_LOAD
        "ÉCRAN ET RÉTROÉCLAIRAGE",                      // DASH_DISPLAY_BACKLIGHT
        "STOCKAGE ET SSD NVME",                         // DASH_STORAGE_NVME
        "RÉPARTITION MATÉRIELLE",                       // DASH_POWER_SHARE_HW
        "CONSOMMATION PAR PROCESSUS",                   // DASH_POWER_SHARE_PROC
        "CHRONOLOGIE ÉNERGÉTIQUE",                      // DASH_TIMELINE
        "Détails",                                      // DASH_DETAILS
    },
    // 5: AR (Modern Standard Arabic - العربية)
    {
        "تفريغ البطارية",                                // STATUS_DISCHARGING
        "تيار مباشر (مشحون بالكامل)",                    // STATUS_AC_PASSTHROUGH
        "جارٍ الشحن بالتيار",                           // STATUS_AC_CHARGING
        "متصل بالتيار الكهربائي",                        // STATUS_AC_CONNECTED
        "يعمل على البطارية",                            // STATUS_ON_BATTERY
        "متبقي %u دقيقة",                               // BATTERY_TIME_LEFT
        "جارٍ الحساب...",                               // BATTERY_TIME_CALCULATING
        "متصل بالكهرباء (غير محدود)",                   // BATTERY_TIME_UNLIMITED
        "%1 س %2 د",                                    // BATTERY_TIME_HOURS_MINS
        "%1 د",                                         // BATTERY_TIME_MINS
        "أداء فائق (تعزيز 4.1G)",                       // PROFILE_PERFORMANCE_SHORT
        "متوازن",                                       // PROFILE_BALANCED_SHORT
        "توفير ذكي (1.7G)",                             // PROFILE_SMARTSAVE_SHORT
        "توفير فائق (1.4G)",                            // PROFILE_ULTRASAVE_SHORT
        "وضع الأداء العالي (تعزيز 4.1GHz)",             // PROFILE_PERFORMANCE_LONG
        "الوضع المتوازن (الموصى به)",                   // PROFILE_BALANCED_LONG
        "وضع التوفير الذكي (1.7GHz)",                   // PROFILE_SMARTSAVE_LONG
        "وضع التوفير الأقصى (حد 1.4GHz)",               // PROFILE_ULTRASAVE_LONG
        "خمول مستقر (لا استنزاف)",                      // HUD_IDLE_STABLE
        "📈 فتح لوحة معلومات Matrix",                  // ACTION_OPEN_DASHBOARD
        "🔋 فتح تقرير فحص استنزاف البطارية",              // ACTION_OPEN_BATTERY_REPORT
        "📊 فتح مراقب نظام KDE",                        // ACTION_OPEN_SYSMONITOR
        "حزمة المعالج",                                 // DEV_CPU_PKG
        "معالج الرسوميات",                              // DEV_GPU_SILICON
        "الشاشة والإضاءة الخلفية",                      // DEV_DISPLAY
        "التخزين وSSD NVMe",                            // DEV_STORAGE
        "مروحة التبريد",                                // DEV_COOLING_FAN
        "اللوحة الأم / النظام",                          // DEV_PLATFORM
        "حالة خدمة WattCurb المقيمة",                   // CLI_STATUS_HEADER
        "إجمالي استهلاك النظام",                         // CLI_TOTAL_DRAIN
        "استهلاك المعالج",                              // CLI_CPU_DRAIN
        "استهلاك الرسوميات",                            // CLI_GPU_DRAIN
        "مستوى البطارية",                               // CLI_BATTERY_LEVEL
        "تنبيهات النظام",                               // CLI_WAKEUPS
        "مروحة التبريد",                                // CLI_COOLING_FAN
        "إجراءات التهدئة النشطة",                       // CLI_ACTIVE_MITIGATIONS
        "العملية الأكثر استهلاكاً",                     // CLI_TOP_CULPRIT
        "إجمالي الاستهلاك",                             // DASH_TOTAL_DRAIN
        "نظام المعالج والذاكرة",                        // DASH_CPU_MEM_SUBSYSTEM
        "البطارية ومصدر الطاقة",                        // DASH_BATTERY_POWER_SUPPLY
        "الرسوميات والتحميل",                           // DASH_GPU_SILICON_LOAD
        "الشاشة والإضاءة",                              // DASH_DISPLAY_BACKLIGHT
        "التخزين وNVMe",                                // DASH_STORAGE_NVME
        "توزيع طاقة الأجهزة",                           // DASH_POWER_SHARE_HW
        "استهلاك طاقة العمليات",                        // DASH_POWER_SHARE_PROC
        "مخطط استهلاك الطاقة",                          // DASH_TIMELINE
        "التفاصيل",                                     // DASH_DETAILS
    },
    // 6: BN (Bengali - বাংলা)
    {
        "ডিসচার্জ হচ্ছে",                                 // STATUS_DISCHARGING
        "সরাসরি এসি (সম্পূর্ণ চার্জ)",                   // STATUS_AC_PASSTHROUGH
        "এসি চার্জ হচ্ছে",                              // STATUS_AC_CHARGING
        "এসি সংযুক্ত",                                  // STATUS_AC_CONNECTED
        "ব্যাটারিতে চলছে",                              // STATUS_ON_BATTERY
        "%u মিনিট বাকি",                                // BATTERY_TIME_LEFT
        "হিসাব করা হচ্ছে...",                           // BATTERY_TIME_CALCULATING
        "পাওয়ার সংযুক্ত (সীমাহীন)",                    // BATTERY_TIME_UNLIMITED
        "%1 ঘণ্টা %2 মিনিট",                            // BATTERY_TIME_HOURS_MINS
        "%1 মিনিট",                                     // BATTERY_TIME_MINS
        "পারফরম্যান্স (4.1G বুস্ট)",                     // PROFILE_PERFORMANCE_SHORT
        "ভারসাম্যপূর্ণ",                                 // PROFILE_BALANCED_SHORT
        "স্মার্ট সেভ (1.7G)",                           // PROFILE_SMARTSAVE_SHORT
        "আল্ট্রা সেভ (1.4G)",                           // PROFILE_ULTRASAVE_SHORT
        "হাই পারফরম্যান্স মোড (4.1GHz বুস্ট)",          // PROFILE_PERFORMANCE_LONG
        "ভারসাম্যপূর্ণ মোড (প্রস্তাবিত)",                // PROFILE_BALANCED_LONG
        "স্মার্ট সেভ মোড (1.7GHz)",                     // PROFILE_SMARTSAVE_LONG
        "আল্ট্রা সেভ মোড (1.4GHz সর্বোচ্চ)",             // PROFILE_ULTRASAVE_LONG
        "স্থিতিশীল নিষ্ক্রিয় (কোনো অপচয় নেই)",        // HUD_IDLE_STABLE
        "📈 ম্যাট্রিক্স ড্যাশবোর্ড খুলুন",             // ACTION_OPEN_DASHBOARD
        "🔋 ব্যাটারি ড্রেন অডিট রিপোর্ট খুলুন",           // ACTION_OPEN_BATTERY_REPORT
        "📊 KDE সিস্টেম মনিটর খুলুন",                    // ACTION_OPEN_SYSMONITOR
        "সিপিইউ প্যাকেজ",                               // DEV_CPU_PKG
        "জিপিইউ সিলিকন",                                // DEV_GPU_SILICON
        "ডিসপ্লে ও ব্যাকলাইট",                          // DEV_DISPLAY
        "স্টোরেজ ও এনভিএমই এসএসডি",                     // DEV_STORAGE
        "কুলিং ফ্যান",                                  // DEV_COOLING_FAN
        "প্ল্যাটফর্ম / মাদারবোর্ড",                     // DEV_PLATFORM
        "WattCurb রেসিডেন্ট ডেমন স্ট্যাটাস",             // CLI_STATUS_HEADER
        "মোট সিস্টেম পাওয়ার ব্যবহার",                  // CLI_TOTAL_DRAIN
        "সিপিইউ প্যাকেজ পাওয়ার",                       // CLI_CPU_DRAIN
        "জিপিইউ সিলিকন পাওয়ার",                        // CLI_GPU_DRAIN
        "ব্যাটারির মাত্রা",                             // CLI_BATTERY_LEVEL
        "সিস্টেম ওয়েকআপ",                               // CLI_WAKEUPS
        "কুলিং ফ্যান গতি",                              // CLI_COOLING_FAN
        "সক্রিয় শক্তি সাশ্রয় নীতি",                   // CLI_ACTIVE_MITIGATIONS
        "সর্বোচ্চ বিদ্যুৎ খরচকারী প্রসেস",              // CLI_TOP_CULPRIT
        "মোট ব্যবহার",                                  // DASH_TOTAL_DRAIN
        "সিপিইউ ও মেমরি সাবসিস্টেম",                    // DASH_CPU_MEM_SUBSYSTEM
        "ব্যাটারি ও পাওয়ার সাপ্লাই",                   // DASH_BATTERY_POWER_SUPPLY
        "জিপিইউ সিলিকন ও লোড",                          // DASH_GPU_SILICON_LOAD
        "ডিসপ্লে ও ব্যাকলাইট",                          // DASH_DISPLAY_BACKLIGHT
        "স্টোরেজ ও এনভিএমই",                            // DASH_STORAGE_NVME
        "হার্ডওয়্যার শক্তি ভাগ",                       // DASH_POWER_SHARE_HW
        "প্রসেস শক্তি খরচ",                             // DASH_POWER_SHARE_PROC
        "পাওয়ার ব্যবহারের সময়রেখা",                    // DASH_TIMELINE
        "বিস্তারিত",                                    // DASH_DETAILS
    },
    // 7: PT (Portuguese - Português)
    {
        "Descarregando",                                // STATUS_DISCHARGING
        "CA Direto (Carga completa)",                   // STATUS_AC_PASSTHROUGH
        "Carregando na tomada",                         // STATUS_AC_CHARGING
        "Conectado à tomada",                           // STATUS_AC_CONNECTED
        "Na bateria",                                   // STATUS_ON_BATTERY
        "%u min restantes",                             // BATTERY_TIME_LEFT
        "Calculando...",                                // BATTERY_TIME_CALCULATING
        "Conectado à energia (Ilimitado)",              // BATTERY_TIME_UNLIMITED
        "%1h %2m",                                      // BATTERY_TIME_HOURS_MINS
        "%1m",                                          // BATTERY_TIME_MINS
        "Desempenho (4.1G Boost)",                      // PROFILE_PERFORMANCE_SHORT
        "Equilibrado",                                  // PROFILE_BALANCED_SHORT
        "Economia Inteligente (1.7G)",                  // PROFILE_SMARTSAVE_SHORT
        "Ultra Economia (1.4G)",                        // PROFILE_ULTRASAVE_SHORT
        "Modo Alto Desempenho (Boost 4.1GHz)",          // PROFILE_PERFORMANCE_LONG
        "Modo Equilibrado (Padrão Recomendado)",        // PROFILE_BALANCED_LONG
        "Modo Economia Inteligente (1.7GHz)",           // PROFILE_SMARTSAVE_LONG
        "Modo Ultra Economia (Limite 1.4GHz)",          // PROFILE_ULTRASAVE_LONG
        "Ocioso estável (Sem vazamentos)",              // HUD_IDLE_STABLE
        "📈 Abrir Painel Matrix",                       // ACTION_OPEN_DASHBOARD
        "🔋 Abrir Relatório de Auditoria de Bateria",     // ACTION_OPEN_BATTERY_REPORT
        "📊 Abrir Monitor do Sistema KDE",              // ACTION_OPEN_SYSMONITOR
        "Pacote CPU",                                   // DEV_CPU_PKG
        "Silício GPU",                                  // DEV_GPU_SILICON
        "Tela e Luz de Fundo",                          // DEV_DISPLAY
        "Armazenamento e SSD NVMe",                     // DEV_STORAGE
        "Ventoinha",                                    // DEV_COOLING_FAN
        "Plataforma / Placa-mãe",                       // DEV_PLATFORM
        "Status do Daemon Residente WattCurb",          // CLI_STATUS_HEADER
        "Consumo total do sistema",                     // CLI_TOTAL_DRAIN
        "Consumo do pacote CPU",                        // CLI_CPU_DRAIN
        "Consumo da GPU",                               // CLI_GPU_DRAIN
        "Nível da bateria",                             // CLI_BATTERY_LEVEL
        "Acordares do sistema",                         // CLI_WAKEUPS
        "Ventoinha",                                    // CLI_COOLING_FAN
        "Mitigações ativas",                            // CLI_ACTIVE_MITIGATIONS
        "Maior consumidor de energia",                  // CLI_TOP_CULPRIT
        "CONSUMO TOTAL",                                // DASH_TOTAL_DRAIN
        "SUBSISTEMA CPU E MEMÓRIA",                     // DASH_CPU_MEM_SUBSYSTEM
        "BATERIA E FONTE DE ENERGIA",                   // DASH_BATTERY_POWER_SUPPLY
        "SILÍCIO GPU E CARGA",                          // DASH_GPU_SILICON_LOAD
        "TELA E LUZ DE FUNDO",                          // DASH_DISPLAY_BACKLIGHT
        "ARMAZENAMENTO E SSD NVME",                     // DASH_STORAGE_NVME
        "DISTRIBUIÇÃO DE ENERGIA DE HARDWARE",          // DASH_POWER_SHARE_HW
        "CONSUMO POR PROCESSOS",                        // DASH_POWER_SHARE_PROC
        "LINHA DO TEMPO DE ENERGIA",                    // DASH_TIMELINE
        "Detalhes",                                     // DASH_DETAILS
    },
    // 8: RU (Russian - Русский)
    {
        "Разрядка",                                     // STATUS_DISCHARGING
        "Питание от сети (Заряжено)",                   // STATUS_AC_PASSTHROUGH
        "Зарядка от сети",                              // STATUS_AC_CHARGING
        "Подключено к сети",                            // STATUS_AC_CONNECTED
        "Работа от батареи",                            // STATUS_ON_BATTERY
        "Осталось %u мин",                              // BATTERY_TIME_LEFT
        "Вычисление...",                                // BATTERY_TIME_CALCULATING
        "Питание от сети (Неограниченно)",              // BATTERY_TIME_UNLIMITED
        "%1 ч %2 мин",                                  // BATTERY_TIME_HOURS_MINS
        "%1 мин",                                       // BATTERY_TIME_MINS
        "Производительность (4.1G Буст)",               // PROFILE_PERFORMANCE_SHORT
        "Сбалансированный",                             // PROFILE_BALANCED_SHORT
        "Умное энергосбережение (1.7G)",                // PROFILE_SMARTSAVE_SHORT
        "Ультра энергосбережение (1.4G)",               // PROFILE_ULTRASAVE_SHORT
        "Высокая производительность (Буст 4.1GHz)",     // PROFILE_PERFORMANCE_LONG
        "Сбалансированный режим (Рекомендуется)",       // PROFILE_BALANCED_LONG
        "Умное энергосбережение (1.7GHz)",              // PROFILE_SMARTSAVE_LONG
        "Ультра энергосбережение (Лимит 1.4GHz)",       // PROFILE_ULTRASAVE_LONG
        "Стабильный простой (Без утечек)",              // HUD_IDLE_STABLE
        "📈 Открыть матричную панель управления",       // ACTION_OPEN_DASHBOARD
        "🔋 Открыть отчет об аудите расхода батареи",     // ACTION_OPEN_BATTERY_REPORT
        "📊 Открыть системный монитор KDE",             // ACTION_OPEN_SYSMONITOR
        "Корпус CPU",                                   // DEV_CPU_PKG
        "Кристалл GPU",                                 // DEV_GPU_SILICON
        "Экран и подсветка",                            // DEV_DISPLAY
        "Накопитель и NVMe SSD",                        // DEV_STORAGE
        "Вентилятор охлаждения",                        // DEV_COOLING_FAN
        "Платформа / Материнская плата",                // DEV_PLATFORM
        "Состояние демона WattCurb",                    // CLI_STATUS_HEADER
        "Общее энергопотребление системы",              // CLI_TOTAL_DRAIN
        "Потребление процессора",                       // CLI_CPU_DRAIN
        "Потребление видеокарты",                       // CLI_GPU_DRAIN
        "Уровень заряда",                               // CLI_BATTERY_LEVEL
        "Пробуждений системы",                          // CLI_WAKEUPS
        "Вентилятор охлаждения",                        // CLI_COOLING_FAN
        "Активные оптимизации",                         // CLI_ACTIVE_MITIGATIONS
        "Главный потребитель энергии",                  // CLI_TOP_CULPRIT
        "ОБЩЕЕ ПОТРЕБЛЕНИЕ",                            // DASH_TOTAL_DRAIN
        "ПОДСИСТЕМА CPU И ПАМЯТИ",                      // DASH_CPU_MEM_SUBSYSTEM
        "АККУМУЛЯТОР И ПИТАНИЕ",                        // DASH_BATTERY_POWER_SUPPLY
        "ВИДЕОКАРТА И НАГРУЗКА",                        // DASH_GPU_SILICON_LOAD
        "ДИСПЛЕЙ И ПОДСВЕТКА",                          // DASH_DISPLAY_BACKLIGHT
        "НАКОПИТЕЛЬ И NVME SSD",                        // DASH_STORAGE_NVME
        "РАСПРЕДЕЛЕНИЕ ЭНЕРГИИ ПО ОБОРУДОВАНИЮ",        // DASH_POWER_SHARE_HW
        "ПОТРЕБЛЕНИЕ ЭНЕРГИИ ПРОЦЕССАМИ",               // DASH_POWER_SHARE_PROC
        "ГРАФИК ЭНЕРГОПОТРЕБЛЕНИЯ",                     // DASH_TIMELINE
        "Подробности",                                  // DASH_DETAILS
    },
    // 9: UR (Urdu - اردو)
    {
        "ڈسچارج ہو رہا ہے",                              // STATUS_DISCHARGING
        "براہ راست AC (مکمل چارج)",                     // STATUS_AC_PASSTHROUGH
        "AC چارج ہو رہا ہے",                            // STATUS_AC_CHARGING
        "AC منسلک ہے",                                  // STATUS_AC_CONNECTED
        "بیٹری پر",                                      // STATUS_ON_BATTERY
        "%u منٹ باقی",                                  // BATTERY_TIME_LEFT
        "حساب کتاب جاری ہے...",                         // BATTERY_TIME_CALCULATING
        "پاور منسلک ہے (لامحدود)",                      // BATTERY_TIME_UNLIMITED
        "%1 گھنٹہ %2 منٹ",                              // BATTERY_TIME_HOURS_MINS
        "%1 منٹ",                                       // BATTERY_TIME_MINS
        "کارکردگی (4.1G بوسٹ)",                         // PROFILE_PERFORMANCE_SHORT
        "متوازن",                                       // PROFILE_BALANCED_SHORT
        "سمارٹ سیو (1.7G)",                             // PROFILE_SMARTSAVE_SHORT
        "الٹرا سیو (1.4G)",                             // PROFILE_ULTRASAVE_SHORT
        "اعلیٰ کارکردگی کا موڈ (4.1GHz بوسٹ)",          // PROFILE_PERFORMANCE_LONG
        "متوازن موڈ (تجویز کردہ)",                      // PROFILE_BALANCED_LONG
        "سمارٹ پاور سیور موڈ (1.7GHz)",                 // PROFILE_SMARTSAVE_LONG
        "الٹرا پاور سیور موڈ (1.4GHz حد)",              // PROFILE_ULTRASAVE_LONG
        "مستحکم بیکار (کوئی رساو نہیں)",                // HUD_IDLE_STABLE
        "📈 میٹرکس ڈیش بورڈ کھولیں",                   // ACTION_OPEN_DASHBOARD
        "🔋 بیٹری ڈرین آڈٹ رپورٹ کھولیں",               // ACTION_OPEN_BATTERY_REPORT
        "📊 KDE سسٹم مانیٹر کھولیں",                    // ACTION_OPEN_SYSMONITOR
        "سی پی یو پیکیج",                               // DEV_CPU_PKG
        "جی پی یو سلیکون",                              // DEV_GPU_SILICON
        "ڈسپلے اور بیک لائٹ",                           // DEV_DISPLAY
        "اسٹوریج اور NVMe SSD",                         // DEV_STORAGE
        "کولنگ فین",                                    // DEV_COOLING_FAN
        "پلیٹ فارم / مدر بورڈ",                         // DEV_PLATFORM
        "WattCurb ڈیمن بائنری اسٹیٹس",                  // CLI_STATUS_HEADER
        "مجموعی پاور کا اخراج",                         // CLI_TOTAL_DRAIN
        "سی پی یو کا اخراج",                            // CLI_CPU_DRAIN
        "جی پی یو کا اخراج",                            // CLI_GPU_DRAIN
        "بیٹری کی سطح",                                 // CLI_BATTERY_LEVEL
        "سسٹم بیداری کی شرح",                          // CLI_WAKEUPS
        "کولنگ فین",                                    // CLI_COOLING_FAN
        "فعال تخفیف پالیسیاں",                          // CLI_ACTIVE_MITIGATIONS
        "سب سے زیادہ پاور خرچ کرنے والا پروسیس",        // CLI_TOP_CULPRIT
        "مجموعی اخراج",                                 // DASH_TOTAL_DRAIN
        "سی پی یو اور میموری سب سسٹم",                  // DASH_CPU_MEM_SUBSYSTEM
        "بیٹری اور پاور سپلائی",                        // DASH_BATTERY_POWER_SUPPLY
        "جی پی یو اور لوڈ",                             // DASH_GPU_SILICON_LOAD
        "ڈسپلے اور بیک لائٹ",                           // DASH_DISPLAY_BACKLIGHT
        "اسٹوریج اور NVMe",                             // DASH_STORAGE_NVME
        "ہارڈویئر پاور شیئرز",                          // DASH_POWER_SHARE_HW
        "پروسیس توانائی کا استعمال",                     // DASH_POWER_SHARE_PROC
        "پاور کے اخراج کی ٹائم لائن",                   // DASH_TIMELINE
        "تفصیلات",                                      // DASH_DETAILS
    },
    // 10: ID (Indonesian - Bahasa Indonesia)
    {
        "Mengosongkan",                                 // STATUS_DISCHARGING
        "AC Langsung (Penuh)",                          // STATUS_AC_PASSTHROUGH
        "Mengisi Daya AC",                              // STATUS_AC_CHARGING
        "AC Terhubung",                                 // STATUS_AC_CONNECTED
        "Menggunakan Baterai",                          // STATUS_ON_BATTERY
        "Tersisa %u menit",                             // BATTERY_TIME_LEFT
        "Menghitung...",                                // BATTERY_TIME_CALCULATING
        "Daya Terhubung (Tanpa Batas)",                 // BATTERY_TIME_UNLIMITED
        "%1j %2m",                                      // BATTERY_TIME_HOURS_MINS
        "%1m",                                          // BATTERY_TIME_MINS
        "Performa (4.1G Boost)",                        // PROFILE_PERFORMANCE_SHORT
        "Seimbang",                                     // PROFILE_BALANCED_SHORT
        "Hemat Pintar (1.7G)",                          // PROFILE_SMARTSAVE_SHORT
        "Ultra Hemat (1.4G)",                           // PROFILE_ULTRASAVE_SHORT
        "Mode Performa Tinggi (Boost 4.1GHz)",          // PROFILE_PERFORMANCE_LONG
        "Mode Seimbang (Direkomendasikan)",             // PROFILE_BALANCED_LONG
        "Mode Hemat Pintar (1.7GHz)",                   // PROFILE_SMARTSAVE_LONG
        "Mode Ultra Hemat (Batas 1.4GHz)",              // PROFILE_ULTRASAVE_LONG
        "Idle Stabil (Tanpa Kebocoran)",                // HUD_IDLE_STABLE
        "📈 Buka Dasbor Matriks",                       // ACTION_OPEN_DASHBOARD
        "🔋 Buka Laporan Audit Pengurasan Baterai",      // ACTION_OPEN_BATTERY_REPORT
        "📊 Buka Monitor Sistem KDE",                   // ACTION_OPEN_SYSMONITOR
        "Paket CPU",                                    // DEV_CPU_PKG
        "Silikon GPU",                                  // DEV_GPU_SILICON
        "Layar & Lampu Latar",                          // DEV_DISPLAY
        "Penyimpanan & SSD NVMe",                       // DEV_STORAGE
        "Kipas Pendingin",                              // DEV_COOLING_FAN
        "Platform / Motherboard",                       // DEV_PLATFORM
        "Status Daemon Residen WattCurb",               // CLI_STATUS_HEADER
        "Total Penggunaan Daya Sistem",                 // CLI_TOTAL_DRAIN
        "Daya Paket CPU",                               // CLI_CPU_DRAIN
        "Daya Silikon GPU",                             // CLI_GPU_DRAIN
        "Tingkat Baterai",                              // CLI_BATTERY_LEVEL
        "Wakeup Sistem",                                // CLI_WAKEUPS
        "Kipas Pendingin",                              // CLI_COOLING_FAN
        "Mitigasi Aktif",                               // CLI_ACTIVE_MITIGATIONS
        "Proses Penguras Daya Terbesar",                // CLI_TOP_CULPRIT
        "TOTAL PENGGUNAAN DAYA",                        // DASH_TOTAL_DRAIN
        "SUBSISTEM CPU & MEMORI",                       // DASH_CPU_MEM_SUBSYSTEM
        "BATERAI & CATU DAYA",                          // DASH_BATTERY_POWER_SUPPLY
        "SILIKON GPU & BEBAN",                          // DASH_GPU_SILICON_LOAD
        "LAYAR & LAMPU LATAR",                          // DASH_DISPLAY_BACKLIGHT
        "PENYIMPANAN & SSD NVME",                       // DASH_STORAGE_NVME
        "DISTRIBUSI DAYA PERANGKAT KERAS",              // DASH_POWER_SHARE_HW
        "KONSUMSI ENERGI PROSES",                       // DASH_POWER_SHARE_PROC
        "GARIS WAKTU PENGGUNAAN DAYA",                  // DASH_TIMELINE
        "Detail",                                       // DASH_DETAILS
    },
    // 11: DE (German - Deutsch)
    {
        "Entladen",                                     // STATUS_DISCHARGING
        "Netzbetrieb direkt (Voll geladen)",            // STATUS_AC_PASSTHROUGH
        "Netzladung aktiv",                             // STATUS_AC_CHARGING
        "Netzteil angeschlossen",                       // STATUS_AC_CONNECTED
        "Akkubetrieb",                                  // STATUS_ON_BATTERY
        "%u Min. verbleibend",                          // BATTERY_TIME_LEFT
        "Berechne...",                                  // BATTERY_TIME_CALCULATING
        "Netzteil verbunden (Unbegrenzt)",              // BATTERY_TIME_UNLIMITED
        "%1 Std. %2 Min.",                              // BATTERY_TIME_HOURS_MINS
        "%1 Min.",                                      // BATTERY_TIME_MINS
        "Leistung (4.1G Boost)",                        // PROFILE_PERFORMANCE_SHORT
        "Ausgewogen",                                   // PROFILE_BALANCED_SHORT
        "Smart-Sparen (1.7G)",                          // PROFILE_SMARTSAVE_SHORT
        "Ultra-Sparen (1.4G)",                          // PROFILE_ULTRASAVE_SHORT
        "Hochleistungsmodus (Boost 4.1GHz)",            // PROFILE_PERFORMANCE_LONG
        "Ausgewogener Modus (Empfohlen)",               // PROFILE_BALANCED_LONG
        "Smart-Sparmodus (1.7GHz)",                     // PROFILE_SMARTSAVE_LONG
        "Ultra-Sparmodus (1.4GHz Limit)",               // PROFILE_ULTRASAVE_LONG
        "Leerlauf stabil (Keine Lecks)",                // HUD_IDLE_STABLE
        "📈 Matrix-Dashboard öffnen",                   // ACTION_OPEN_DASHBOARD
        "🔋 Batterie-Audit-Bericht öffnen",               // ACTION_OPEN_BATTERY_REPORT
        "📊 KDE-Systemmonitor öffnen",                  // ACTION_OPEN_SYSMONITOR
        "CPU-Paket",                                    // DEV_CPU_PKG
        "GPU-Silizium",                                 // DEV_GPU_SILICON
        "Bildschirm & Hintergrundbeleuchtung",          // DEV_DISPLAY
        "Speicher & NVMe-SSD",                          // DEV_STORAGE
        "Lüfter",                                       // DEV_COOLING_FAN
        "Plattform / Mainboard",                        // DEV_PLATFORM
        "Status des residenten WattCurb-Daemons",        // CLI_STATUS_HEADER
        "Gesamtsystem-Leistungsaufnahme",               // CLI_TOTAL_DRAIN
        "CPU-Paket-Leistung",                           // CLI_CPU_DRAIN
        "GPU-Leistung",                                 // CLI_GPU_DRAIN
        "Akkustand",                                    // CLI_BATTERY_LEVEL
        "System-Wakeups",                               // CLI_WAKEUPS
        "Lüfterdrehzahl",                               // CLI_COOLING_FAN
        "Aktive Sparmaßnahmen",                         // CLI_ACTIVE_MITIGATIONS
        "Haupt-Stromverbraucher",                       // CLI_TOP_CULPRIT
        "GESAMTVERBRAUCH",                              // DASH_TOTAL_DRAIN
        "CPU- & SPEICHER-SUBSYSTEM",                    // DASH_CPU_MEM_SUBSYSTEM
        "AKKU & STROMVERSORGUNG",                       // DASH_BATTERY_POWER_SUPPLY
        "GPU-SILIZIUM & AUSLASTUNG",                    // DASH_GPU_SILICON_LOAD
        "BILDSCHIRM & BELEUCHTUNG",                     // DASH_DISPLAY_BACKLIGHT
        "SPEICHER & NVME-SSD",                          // DASH_STORAGE_NVME
        "HARDWARE-LEISTUNGSAUFTEILUNG",                 // DASH_POWER_SHARE_HW
        "ENERGIEVERBRAUCH NACH PROZESSEN",              // DASH_POWER_SHARE_PROC
        "LEISTUNGSVERBRAUCHS-ZEITLEISTE",               // DASH_TIMELINE
        "Details",                                      // DASH_DETAILS
    },
    // 12: KO (Korean - 한국어)
    {
        "방전 중",                                       // STATUS_DISCHARGING
        "AC 직결 (완충)",                                // STATUS_AC_PASSTHROUGH
        "AC 충전 중",                                    // STATUS_AC_CHARGING
        "AC 연결",                                       // STATUS_AC_CONNECTED
        "배터리 사용 중",                                // STATUS_ON_BATTERY
        "%u분 남음",                                     // BATTERY_TIME_LEFT
        "계산 중...",                                    // BATTERY_TIME_CALCULATING
        "전원 연결됨 (무제한)",                          // BATTERY_TIME_UNLIMITED
        "%1시간 %2분",                                   // BATTERY_TIME_HOURS_MINS
        "%1분",                                          // BATTERY_TIME_MINS
        "Performance (4.1G 언락)",                       // PROFILE_PERFORMANCE_SHORT
        "Balanced (균형)",                               // PROFILE_BALANCED_SHORT
        "SmartSave (절전 1.7G)",                         // PROFILE_SMARTSAVE_SHORT
        "UltraSave (극저전력 1.4G)",                     // PROFILE_ULTRASAVE_SHORT
        "Performance (고성능 모드 - 4.1GHz Boost)",      // PROFILE_PERFORMANCE_LONG
        "Balanced (균형 모드 - 기본 권장)",              // PROFILE_BALANCED_LONG
        "Smart Save (스마트 절전 모드 - 1.7GHz)",        // PROFILE_SMARTSAVE_LONG
        "Ultra Save (초절전 모드 - 1.4GHz 상한)",        // PROFILE_ULTRASAVE_LONG
        "유휴 안정 (누수 없음)",                         // HUD_IDLE_STABLE
        "📈 정밀 분석 매트릭 창 열기 (Matrix Dashboard)",// ACTION_OPEN_DASHBOARD
        "🔋 배터리 정밀 분석 리포트 열기 (Battery Audit Report)", // ACTION_OPEN_BATTERY_REPORT
        "📊 KDE 시스템 모니터 열기 (System Monitor)",    // ACTION_OPEN_SYSMONITOR
        "CPU 패키지",                                    // DEV_CPU_PKG
        "GPU 실리콘",                                    // DEV_GPU_SILICON
        "디스플레이 & 백라이트",                          // DEV_DISPLAY
        "스토리지 & NVMe SSD",                           // DEV_STORAGE
        "냉각 팬",                                       // DEV_COOLING_FAN
        "플랫폼 / 메인보드",                             // DEV_PLATFORM
        "WattCurb 상주 데몬 바이너리 상태",              // CLI_STATUS_HEADER
        "총 시스템 전력 소모",                           // CLI_TOTAL_DRAIN
        "CPU 패키지 전력",                               // CLI_CPU_DRAIN
        "GPU 실리콘 전력",                               // CLI_GPU_DRAIN
        "배터리 잔량",                                   // CLI_BATTERY_LEVEL
        "시스템 웨이크업",                               // CLI_WAKEUPS
        "냉각 팬 회전수",                                // CLI_COOLING_FAN
        "활성 완화 정책",                                // CLI_ACTIVE_MITIGATIONS
        "최대 전력 소모 프로세스",                       // CLI_TOP_CULPRIT
        "총 시스템 소모 전력",                           // DASH_TOTAL_DRAIN
        "CPU & 메모리 서브시스템",                       // DASH_CPU_MEM_SUBSYSTEM
        "배터리 & 전원 공급",                            // DASH_BATTERY_POWER_SUPPLY
        "GPU 실리콘 & 부하율",                           // DASH_GPU_SILICON_LOAD
        "디스플레이 & 백라이트",                          // DASH_DISPLAY_BACKLIGHT
        "스토리지 & NVMe SSD",                           // DASH_STORAGE_NVME
        "하드웨어 전력 점유율",                          // DASH_POWER_SHARE_HW
        "프로세스 에너지 소모",                          // DASH_POWER_SHARE_PROC
        "실시간 전력 소모 타임라인",                     // DASH_TIMELINE
        "상세",                                          // DASH_DETAILS
    },
    // 13: JA (Japanese - 日本語)
    {
        "放電中",                                        // STATUS_DISCHARGING
        "ACパススルー (満充電)",                          // STATUS_AC_PASSTHROUGH
        "AC充電中",                                      // STATUS_AC_CHARGING
        "AC電源接続中",                                  // STATUS_AC_CONNECTED
        "バッテリー駆動中",                              // STATUS_ON_BATTERY
        "残り %u 分",                                    // BATTERY_TIME_LEFT
        "計算中...",                                     // BATTERY_TIME_CALCULATING
        "AC接続中 (制限なし)",                           // BATTERY_TIME_UNLIMITED
        "%1時間 %2分",                                   // BATTERY_TIME_HOURS_MINS
        "%1分",                                          // BATTERY_TIME_MINS
        "パフォーマンス (4.1G ブースト)",                 // PROFILE_PERFORMANCE_SHORT
        "バランス",                                      // PROFILE_BALANCED_SHORT
        "スマート節電 (1.7G)",                           // PROFILE_SMARTSAVE_SHORT
        "ウルトラ省電力 (1.4G)",                         // PROFILE_ULTRASAVE_SHORT
        "パフォーマンスモード (4.1GHz ブースト)",          // PROFILE_PERFORMANCE_LONG
        "バランスモード (デフォルト推奨)",               // PROFILE_BALANCED_LONG
        "スマート節電モード (1.7GHz)",                   // PROFILE_SMARTSAVE_LONG
        "ウルトラ省電力モード (1.4GHz 上限)",            // PROFILE_ULTRASAVE_LONG
        "アイドル安定 (リークなし)",                     // HUD_IDLE_STABLE
        "📈 マトリックスダッシュボードを開く",           // ACTION_OPEN_DASHBOARD
        "🔋 バッテリー詳細診断レポートを開く",           // ACTION_OPEN_BATTERY_REPORT
        "📊 KDE システムモニターを開く",                // ACTION_OPEN_SYSMONITOR
        "CPU パッケージ",                                // DEV_CPU_PKG
        "GPU シリコン",                                  // DEV_GPU_SILICON
        "ディスプレイ & バックライト",                    // DEV_DISPLAY
        "ストレージ & NVMe SSD",                         // DEV_STORAGE
        "冷却ファン",                                    // DEV_COOLING_FAN
        "プラットフォーム / マザーボード",               // DEV_PLATFORM
        "WattCurb 常駐デーモン稼働状態",                 // CLI_STATUS_HEADER
        "システム総消費電力",                            // CLI_TOTAL_DRAIN
        "CPU パッケージ消費電力",                        // CLI_CPU_DRAIN
        "GPU シリコン消費電力",                          // CLI_GPU_DRAIN
        "バッテリー残量",                                // CLI_BATTERY_LEVEL
        "システムウェイクアップ",                        // CLI_WAKEUPS
        "冷却ファン回転数",                              // CLI_COOLING_FAN
        "アクティブな緩和ポリシー",                      // CLI_ACTIVE_MITIGATIONS
        "最大電力消費プロセス",                          // CLI_TOP_CULPRIT
        "総消費電力",                                    // DASH_TOTAL_DRAIN
        "CPU & メモリサブシステム",                       // DASH_CPU_MEM_SUBSYSTEM
        "バッテリー & 電源供給",                          // DASH_BATTERY_POWER_SUPPLY
        "GPU シリコン & 負荷率",                         // DASH_GPU_SILICON_LOAD
        "ディスプレイ & バックライト",                    // DASH_DISPLAY_BACKLIGHT
        "ストレージ & NVMe SSD",                         // DASH_STORAGE_NVME
        "ハードウェア電力占有率",                        // DASH_POWER_SHARE_HW
        "プロセスエネルギー消費",                        // DASH_POWER_SHARE_PROC
        "リアルタイム電力消費タイムライン",              // DASH_TIMELINE
        "詳細",                                          // DASH_DETAILS
    }
};

static constexpr const char* const LANGUAGE_CODES[static_cast<size_t>(Language::COUNT)] = {
    "en", "zh", "hi", "es", "fr", "ar", "bn", "pt", "ru", "ur", "id", "de", "ko", "ja"
};

static constexpr const char* const LANGUAGE_NAMES[static_cast<size_t>(Language::COUNT)] = {
    "English",
    "简体中文",
    "हिन्दी",
    "Español",
    "Français",
    "العربية",
    "বাংলা",
    "Português",
    "Русский",
    "اردو",
    "Bahasa Indonesia",
    "Deutsch",
    "한국어",
    "日本語"
};

} // anonymous namespace

Language get_active_language() noexcept {
    return g_active_lang.load(std::memory_order_relaxed);
}

void set_language(Language lang) noexcept {
    if (static_cast<size_t>(lang) < static_cast<size_t>(Language::COUNT)) {
        g_active_lang.store(lang, std::memory_order_relaxed);
    }
}

const char* get_language_code(Language lang) noexcept {
    auto idx = static_cast<size_t>(lang);
    if (idx < static_cast<size_t>(Language::COUNT)) {
        return LANGUAGE_CODES[idx];
    }
    return "en";
}

const char* get_language_name(Language lang) noexcept {
    auto idx = static_cast<size_t>(lang);
    if (idx < static_cast<size_t>(Language::COUNT)) {
        return LANGUAGE_NAMES[idx];
    }
    return "English";
}

const char* tr(StringId id, Language lang) noexcept {
    auto l_idx = static_cast<size_t>(lang);
    auto s_idx = static_cast<size_t>(id);
    if (l_idx >= static_cast<size_t>(Language::COUNT) || s_idx >= static_cast<size_t>(StringId::COUNT)) {
        return "";
    }
    const char* str = STRING_TABLE[l_idx][s_idx];
    if (str && str[0] != '\0') {
        return str;
    }
    // Fallback to English
    return STRING_TABLE[static_cast<size_t>(Language::EN)][s_idx];
}

const char* tr(StringId id) noexcept {
    return tr(id, g_active_lang.load(std::memory_order_relaxed));
}

std::optional<Language> parse_language_code(std::string_view code) noexcept {
    while (!code.empty() && std::isspace(static_cast<unsigned char>(code.front()))) {
        code.remove_prefix(1);
    }
    if (code.empty()) return std::nullopt;

    // Extract language prefix up to '_', '.', '@', '-', or end
    size_t prefix_len = 0;
    while (prefix_len < code.size()) {
        char c = code[prefix_len];
        if (c == '_' || c == '.' || c == '@' || c == '-' || std::isspace(static_cast<unsigned char>(c))) {
            break;
        }
        ++prefix_len;
    }
    if (prefix_len == 0) return std::nullopt;

    char buf[16]{};
    size_t copy_len = prefix_len < sizeof(buf) - 1 ? prefix_len : sizeof(buf) - 1;
    for (size_t i = 0; i < copy_len; ++i) {
        buf[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(code[i])));
    }
    std::string_view p(buf, copy_len);

    if (p == "ko" || p == "kor" || p == "korean") return Language::KO;
    if (p == "ja" || p == "jpn" || p == "japanese") return Language::JA;
    if (p == "zh" || p == "zho" || p == "chi" || p == "chinese") return Language::ZH;
    if (p == "hi" || p == "hin" || p == "hindi") return Language::HI;
    if (p == "es" || p == "spa" || p == "spanish" || p == "espanol") return Language::ES;
    if (p == "fr" || p == "fra" || p == "fre" || p == "french" || p == "francais") return Language::FR;
    if (p == "ar" || p == "ara" || p == "arabic") return Language::AR;
    if (p == "bn" || p == "ben" || p == "bengali" || p == "bangla") return Language::BN;
    if (p == "pt" || p == "por" || p == "portuguese") return Language::PT;
    if (p == "ru" || p == "rus" || p == "russian") return Language::RU;
    if (p == "ur" || p == "urd" || p == "urdu") return Language::UR;
    if (p == "id" || p == "ind" || p == "indonesian") return Language::ID;
    if (p == "de" || p == "deu" || p == "ger" || p == "german" || p == "deutsch") return Language::DE;
    if (p == "en" || p == "eng" || p == "english") return Language::EN;

    return std::nullopt;
}

Language detect_system_language() noexcept {
    // 1. Explicit user override
    const char* env_wattcurb = std::getenv("WATTCURB_LANG");
    if (env_wattcurb && env_wattcurb[0] != '\0') {
        auto lang = parse_language_code(env_wattcurb);
        if (lang) return *lang;
    }

    // 2. Standard POSIX locale environment variables
    const char* env_locale = std::getenv("LC_ALL");
    if (!env_locale || env_locale[0] == '\0') {
        env_locale = std::getenv("LC_MESSAGES");
    }
    if (!env_locale || env_locale[0] == '\0') {
        env_locale = std::getenv("LANG");
    }

    if (env_locale && env_locale[0] != '\0') {
        auto lang = parse_language_code(env_locale);
        if (lang) return *lang;
    }

    // 3. Fallback default
    return Language::EN;
}

void init_from_system() noexcept {
    Language lang = detect_system_language();
    set_language(lang);
}

std::optional<StringId> parse_string_key(std::string_view key) noexcept {
    if (key == "STATUS_DISCHARGING") return StringId::STATUS_DISCHARGING;
    if (key == "STATUS_AC_PASSTHROUGH") return StringId::STATUS_AC_PASSTHROUGH;
    if (key == "STATUS_AC_CHARGING") return StringId::STATUS_AC_CHARGING;
    if (key == "STATUS_AC_CONNECTED") return StringId::STATUS_AC_CONNECTED;
    if (key == "STATUS_ON_BATTERY") return StringId::STATUS_ON_BATTERY;
    if (key == "BATTERY_TIME_LEFT") return StringId::BATTERY_TIME_LEFT;
    if (key == "BATTERY_TIME_CALCULATING") return StringId::BATTERY_TIME_CALCULATING;
    if (key == "BATTERY_TIME_UNLIMITED") return StringId::BATTERY_TIME_UNLIMITED;
    if (key == "BATTERY_TIME_HOURS_MINS") return StringId::BATTERY_TIME_HOURS_MINS;
    if (key == "BATTERY_TIME_MINS") return StringId::BATTERY_TIME_MINS;
    if (key == "PROFILE_PERFORMANCE_SHORT") return StringId::PROFILE_PERFORMANCE_SHORT;
    if (key == "PROFILE_BALANCED_SHORT") return StringId::PROFILE_BALANCED_SHORT;
    if (key == "PROFILE_SMARTSAVE_SHORT") return StringId::PROFILE_SMARTSAVE_SHORT;
    if (key == "PROFILE_ULTRASAVE_SHORT") return StringId::PROFILE_ULTRASAVE_SHORT;
    if (key == "PROFILE_PERFORMANCE_LONG") return StringId::PROFILE_PERFORMANCE_LONG;
    if (key == "PROFILE_BALANCED_LONG") return StringId::PROFILE_BALANCED_LONG;
    if (key == "PROFILE_SMARTSAVE_LONG") return StringId::PROFILE_SMARTSAVE_LONG;
    if (key == "PROFILE_ULTRASAVE_LONG") return StringId::PROFILE_ULTRASAVE_LONG;
    if (key == "HUD_IDLE_STABLE") return StringId::HUD_IDLE_STABLE;
    if (key == "ACTION_OPEN_DASHBOARD") return StringId::ACTION_OPEN_DASHBOARD;
    if (key == "ACTION_OPEN_BATTERY_REPORT") return StringId::ACTION_OPEN_BATTERY_REPORT;
    if (key == "ACTION_OPEN_SYSMONITOR") return StringId::ACTION_OPEN_SYSMONITOR;
    if (key == "DEV_CPU_PKG") return StringId::DEV_CPU_PKG;
    if (key == "DEV_GPU_SILICON") return StringId::DEV_GPU_SILICON;
    if (key == "DEV_DISPLAY") return StringId::DEV_DISPLAY;
    if (key == "DEV_STORAGE") return StringId::DEV_STORAGE;
    if (key == "DEV_COOLING_FAN") return StringId::DEV_COOLING_FAN;
    if (key == "DEV_PLATFORM") return StringId::DEV_PLATFORM;
    if (key == "DASH_TOTAL_DRAIN" || key == "TOTAL_DRAIN") return StringId::DASH_TOTAL_DRAIN;
    if (key == "DASH_CPU_MEM_SUBSYSTEM" || key == "CPU_MEM") return StringId::DASH_CPU_MEM_SUBSYSTEM;
    if (key == "DASH_BATTERY_POWER_SUPPLY" || key == "BATTERY") return StringId::DASH_BATTERY_POWER_SUPPLY;
    if (key == "DASH_GPU_SILICON_LOAD" || key == "GPU") return StringId::DASH_GPU_SILICON_LOAD;
    if (key == "DASH_DISPLAY_BACKLIGHT" || key == "DISPLAY") return StringId::DASH_DISPLAY_BACKLIGHT;
    if (key == "DASH_STORAGE_NVME" || key == "STORAGE") return StringId::DASH_STORAGE_NVME;
    if (key == "DASH_POWER_SHARE_HW" || key == "POWER_SHARE_HW") return StringId::DASH_POWER_SHARE_HW;
    if (key == "DASH_POWER_SHARE_PROC" || key == "POWER_SHARE_PROC") return StringId::DASH_POWER_SHARE_PROC;
    if (key == "DASH_TIMELINE" || key == "TIMELINE") return StringId::DASH_TIMELINE;
    if (key == "DASH_DETAILS" || key == "DETAILS") return StringId::DASH_DETAILS;
    return std::nullopt;
}

} // namespace wattcurb::core::l10n
