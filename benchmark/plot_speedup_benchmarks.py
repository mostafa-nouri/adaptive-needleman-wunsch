import pandas as pd
import matplotlib.pyplot as plt
import matplotlib as mpl
import seaborn as sns
import os
import warnings

# غیرفعال کردن نمایش اخطارهای غیرضروری فونت در ترمینال جهت پاکسازی خروجی
warnings.filterwarnings("ignore", category=UserWarning)

# Define file paths matching your complete master harness suite
CSV_FILE = "scientific_evaluation_report.csv"
OUTPUT_PNG = "nw_parallel_speedup_chart.png"
OUTPUT_PDF = "nw_parallel_speedup_chart.pdf"

# --- SEQUENTIAL BASELINE PARAMETER ---
# Set this to your verified single-threaded classic run time
T_SEQ = 5.330559

# 1. Dataset Verification
if not os.path.exists(CSV_FILE):
    print(f"❌ ERROR: Target benchmark database report '{CSV_FILE}' not found.")
    print("Please make sure you have executed your automated experiment harness first.")
    exit(1)

# Load the master spreadsheet database report cleanly
df = pd.read_csv(CSV_FILE)

# تابع مبدل اعشار سیستم بر پایه خط مورب ممیز ایمن در فونت نازنین
def to_persian_digits(num_str):
    persian_map = {
        '0': '۰', '1': '۱', '2': '۲', '3': '۳', '4': '۴',
        '5': '۵', '6': '۶', '7': '۷', '8': '۸', '9': '۹', 
        '.': '/'  # استفاده از ممیز خط مورب جهت جلوگیری از وقوع کادرهای خطای متنی ب
    }
    return ''.join(persian_map.get(char, char) for char in str(num_str))

# 2. Standardize Clean Naming Maps for Presentation Text Labels
PARALLEL_CONFIG = [
    {"csv_key": "OUR_ADAPTIVE_HYSTERESIS",          "clean_name": "تطبیقی با پسماند تجمعی (مقاله حاضر)", "color": "#1B4F72"},
    {"csv_key": "PAPER1_STATIC_ROW_POLLING",        "clean_name": "سرکشی ایستای سطر [۲]",        "color": "#A9DFBF"},
    {"csv_key": "PAPER2_BLOCK_WAVEFRONT_2D",        "clean_name": "جبهه موج دوبعدی بلوکی [۷]",        "color": "#F9E79F"},
    {"csv_key": "PAPER3_STATIC_STAGGERED_BARRIER",  "clean_name": "سد همگام‌سازی پلکانی ایستا [۳]",   "color": "#EDBB99"},
    {"csv_key": "PAPER4_DIAGONAL_BARRIER",          "clean_name": "سد همگام‌سازی قطری [۸]",     "color": "#D2B4DE"},
    {"csv_key": "PAPER5_TASK_POOL_GRID",            "clean_name": "استخر وظایف توزیع شده [۹]",            "color": "#AEB6BF"},
    {"csv_key": "PAPER6_PARALLEL_PREFIX",           "clean_name": "محاسبات پیشوندی موازی [۴]",      "color": "#F5B7B1"}
]

method_renamer = {item["csv_key"]: item["clean_name"] for item in PARALLEL_CONFIG}

condition_renamer = {
    "A_PRISTINE_UNIFORM": "فاز آ\nپیکربندی ایده‌آل یکنواخت",
    "B_ASYMMETRIC_THROTTLED": "فاز ب\nناهمگونی نامتقارن با گلوگاه\nشدت استرس: ۵",
    "C_EXTREME_STALL": "فاز ج\nواماندگی مطلق هسته‌ها\nشدت استرس: ۲۰"
}

df['Method_Name'] = df['Method_Name'].map(method_renamer).fillna(df['Method_Name'])
df['Hardware_Condition'] = df['Hardware_Condition'].map(condition_renamer).fillna(df['Hardware_Condition'])

# Calculate the mean wall-clock times to compile clean statistical averages
df_grouped = df.groupby(['Hardware_Condition', 'Method_Name'], as_index=False)['Wall_Time_Sec'].mean()

# Calculate the empirical Amdahl Speedup Factor (S = T_sequential / T_parallel)
df_grouped['Speedup_Factor'] = T_SEQ / df_grouped['Wall_Time_Sec']

# 3. Configure Professional Academic Canvas & Global Font Engine
plt.figure(figsize=(11, 7))
sns.set_theme(style="whitegrid")

# پایدارسازی زنجیره فونت‌های سیستم برای پشتیبانی از متون ناهمگن
mpl.rcParams['font.sans-serif'] = ['B Nazanin', 'DejaVu Sans', 'Arial', 'Tahoma']
mpl.rcParams['font.family'] = 'sans-serif'

# High-contrast academic color palette focusing heavily on your framework
academic_palette = {item["clean_name"]: item["color"] for item in PARALLEL_CONFIG}
custom_order     = [item["clean_name"] for item in PARALLEL_CONFIG]

# 4. Generate the Grouped Bar Layout for Speedup Factors
ax = sns.barplot(
    data=df_grouped,
    x="Hardware_Condition",
    y="Speedup_Factor",
    hue="Method_Name",
    hue_order=custom_order, # تثبیت جایگاه متد شما در اولین میله سمت چپ نمودار
    palette=academic_palette,
    edgecolor="#2C3E50",
    linewidth=1.2
)

# رسم خط چین قرمز به عنوان خط مرجع مبنا (سرعت اجرای تک هسته‌ای معمولی = ۱)
plt.axhline(y=1.0, color="#CB4335", linestyle="--", linewidth=1.5, label="خط مبنای اجرای ترتیبی تک هسته‌ای (شتاب = ۱/۰)")

# 5. Fine-Tune Typography and Structural Visual Accents
plt.title("مقایسه نرخ شتاب معماری‌های موازی نیدلمن-وانش\nنسبت به الگوریتم ترتیبی کلاسیک خط مبنا\nتحت فازهای مختلف اختلال سخت‌افزاری", 
          fontsize=12, fontweight='bold', pad=15, color="#2C3E50", name='B Nazanin')

plt.xlabel("شرایط مقیاس‌پذیری در شبیه‌سازی سخت‌افزار", fontsize=11, fontweight='bold', labelpad=10, color="#2C3E50", name='B Nazanin')
plt.ylabel("نرخ شتاب موازی استخراج شده (برابر سریع‌تر از تک هسته)", fontsize=11, fontweight='bold', labelpad=10, color="#2C3E50", name='B Nazanin')

# فرمت‌دهی خودکار ارقام محور عمودی (Y) به اعداد فارسی با ممیز ایمن
ax.yaxis.set_major_formatter(plt.FuncFormatter(lambda x, pos: to_persian_digits(f"{x:.1f}")))

# حلقه ایمن بدون اخطار برای پایداری قلم نازنین زیر ستون فازها
for label in ax.get_xticklabels():
    label.set_fontname('B Nazanin')
    label.set_fontsize(10)
    label.set_fontweight('bold')

# Inject speedup value annotations on top of each bar element
for p in ax.patches:
    val = p.get_height()
    if val > 0:
        raw_str = f"{val:.2f}"
        formatted_val = to_persian_digits(raw_str) + " " + "برابر"
        ax.annotate(formatted_val, 
                    (p.get_x() + p.get_width() / 2., val), 
                    ha='center', va='center', 
                    xytext=(0, 8), 
                    textcoords='offset points', 
                    fontsize=8, fontweight='bold', color="#2C3E50")

# Position legend block cleanly out of the data bar zones
plt.legend(title="نوع معماری موازی", 
           prop={'family': 'B Nazanin', 'size': 10, 'weight': 'bold'},
           loc='upper right', bbox_to_anchor=(0.99, 0.99), 
           frameon=True, shadow=True, facecolor="#FDFEFE")

# اعمال قلم فارسی ضخیم بر روی عنوان اصلی کادر راهنما
leg = ax.get_legend()
if leg:
    leg.get_title().set_fontname('B Nazanin')
    leg.get_title().set_fontweight('bold')
    leg.get_title().set_fontsize(11)

# Clean canvas boundary borders
sns.despine(left=True, bottom=True)
plt.tight_layout()

# 6. Export Print-Ready Media Objects
plt.savefig(OUTPUT_PNG, dpi=300)  # High-resolution PNG for manuscripts
plt.savefig(OUTPUT_PDF)          # Vector format ideal for LaTeX compilation

print("=========================================================================")
print(" 📊 SPEEDUP FACTOR PLOT GENERATED SUCCESSFULLY! Figures saved:")
print(f"    ➡️ High-Resolution Image : {OUTPUT_PNG}")
print(f"    ➡️ Publication Vector PDF: {OUTPUT_PDF}")
print("=========================================================================")
