import pandas as pd
import matplotlib.pyplot as plt
import matplotlib as mpl
import seaborn as sns
import os
import warnings

# غیرفعال کردن نمایش اخطارهای فونت در ترمینال جهت تمیزی خروجی
warnings.filterwarnings("ignore", category=UserWarning)

# فایل داده‌ها و خروجی‌ها همگی به طور محلی در پوشه benchmark/ آدرس‌دهی شده‌اند
CSV_FILE = "scientific_evaluation_report.csv"
OUTPUT_PNG = "nw_linear_space_performance.png"
OUTPUT_PDF = "nw_linear_space_performance.pdf"

# 1. Dataset Verification
if not os.path.exists(CSV_FILE):
    print(f"❌ ERROR: Target benchmark database report '{CSV_FILE}' not found.")
    print("Please make sure you have executed 'sudo python3 hpc_experiment_harness.py' from the benchmark/ folder first.")
    exit(1)

# Load the master spreadsheet database report cleanly
df = pd.read_csv(CSV_FILE)

def to_persian_digits(num_str):
    persian_map = {
        '0': '۰', '1': '۱', '2': '۲', '3': '۳', '4': '۴',
        '5': '۵', '6': '۶', '7': '۷', '8': '۸', '9': '۹', 
        '.': '/'  # استفاده از ممیز خط مورب که در نازنین بدون کادر رندر می‌شود
    }
    return ''.join(persian_map.get(char, char) for char in str(num_str))

# 2. Standardize Clean Naming Maps for Presentation Text Labels
# اضافه شدن نسخه حافظه خطی متد شما (OUR_ADAPTIVE_HYSTERESIS_LINEAR) به لایه پیکربندی تک‌مبدایی
PARALLEL_CONFIG = [
    {"csv_key": "OUR_ADAPTIVE_HYSTERESIS",          "clean_name": "تطبیقی با پسماند تجمعی (ماتریس کامل)", "color": "#1F618D"},
    {"csv_key": "OUR_ADAPTIVE_HYSTERESIS_LINEAR",   "clean_name": "تطبیقی با پسماند تجمعی (فضای خطی)", "color": "#1B4F72"},
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

# Apply clean string formatting mappings across columns
df['Method_Name'] = df['Method_Name'].map(method_renamer).fillna(df['Method_Name'])
df['Hardware_Condition'] = df['Hardware_Condition'].map(condition_renamer).fillna(df['Hardware_Condition'])

# Calculate the mean wall-clock times to compile clean statistical averages
df_grouped = df.groupby(['Hardware_Condition', 'Method_Name'], as_index=False)['Wall_Time_Sec'].mean()

# 3. Configure Professional Academic Canvas
plt.figure(figsize=(11, 7))
sns.set_theme(style="whitegrid")

# زنجیره فونت‌های پشتیبان برای کاراکترهایی که در نازنین وجود ندارند
mpl.rcParams['font.sans-serif'] = ['B Nazanin', 'DejaVu Sans', 'Arial', 'Tahoma']
mpl.rcParams['font.family'] = 'sans-serif'

# High-contrast academic color palette focusing heavily on your framework
academic_palette = {item["clean_name"]: item["color"] for item in PARALLEL_CONFIG}
custom_order     = [item["clean_name"] for item in PARALLEL_CONFIG]

# 4. Generate the Grouped Bar Layout
ax = sns.barplot(
    data=df_grouped,
    x="Hardware_Condition",
    y="Wall_Time_Sec",
    hue="Method_Name",
    hue_order=custom_order,
    palette=academic_palette,
    edgecolor="#2C3E50",
    linewidth=1.2
)

# 5. Fine-Tune Typography and Structural Visual Accents
plt.title("زمان اجرای معماری‌های موازی نیدلمن-وانش\nارزیابی کارایی اجرای موازی تحت پروفایل‌های ناهمگونی سخت‌افزاری نامتقارن", 
          fontsize=13, fontweight='bold', pad=15, color="#2C3E50", name='B Nazanin')

plt.xlabel("شرایط مقیاس‌پذیری در شبیه‌سازی سخت‌افزار", fontsize=11, fontweight='bold', labelpad=10, color="#2C3E50", name='B Nazanin')
plt.ylabel("زمان اجرای واقعی محاسبات موازی (ثانیه)", fontsize=11, fontweight='bold', labelpad=10, color="#2C3E50", name='B Nazanin')

# Apply logarithmic y-axis scale to cleanly handle the huge 857s vs 1.4s layout variations
plt.yscale("log")
# Apply Persian Digits formatting to Y-Axis Ticks Labeling Hook
ax.yaxis.set_major_formatter(plt.FuncFormatter(lambda x, pos: to_persian_digits(f"{x:.1f}")))

# حلقه پایداری فونت افقی فازها بدون ایجاد اخطار مکان‌یاب
for label in ax.get_xticklabels():
    label.set_fontname('B Nazanin')
    label.set_fontsize(10)
    label.set_fontweight('bold')

# Inject value annotations on top of each bar element
for p in ax.patches:
    val = p.get_height()
    if val > 0:
        raw_str = f"{val:.2f}"
        formatted_val = to_persian_digits(raw_str) + " " + "ثانیه"
        ax.annotate(formatted_val, 
                    (p.get_x() + p.get_width() / 2., val), 
                    ha='center', va='center', 
                    xytext=(0, 8), 
                    textcoords='offset points', 
                    fontsize=8, fontweight='bold', color="#2C3E50")

# Position legend block out of the data bar zones
plt.legend(title="نوع معماری موازی", 
           prop={'family': 'B Nazanin', 'size': 10, 'weight': 'bold'}, 
           loc='upper left', bbox_to_anchor=(0.01, 0.99), 
           frameon=True, shadow=True, facecolor="#FDFEFE")

# Safely style legend title via object hooks to prevent indirect keyword parameter bugs
leg = ax.get_legend()
if leg:
    leg.get_title().set_fontname('B Nazanin')
    leg.get_title().set_fontweight('bold')
    leg.get_title().set_fontsize(11)

# Clean canvas boundary borders
sns.despine(left=True, bottom=True)
plt.tight_layout()

# 6. Export Print-Ready Media Objects
plt.savefig(OUTPUT_PNG, dpi=300)  # High-resolution PNG for word processors
plt.savefig(OUTPUT_PDF)          # Vector format ideal for LaTeX manuscript uploads

print("=========================================================================")
print(" 🎨 GRAPHING COMPLETELY SUCCESSFUL! Production-grade figures saved:")
print(f"    ➡️ High-Resolution Image : {OUTPUT_PNG}")
print(f"    ➡️ Publication Vector PDF: {OUTPUT_PDF}")
print("=========================================================================")
