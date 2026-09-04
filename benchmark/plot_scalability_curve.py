import pandas as pd
import matplotlib.pyplot as plt
import matplotlib as mpl
import seaborn as sns
import os
import warnings

# Silence non-critical font mapping warnings for clean terminal outputs
warnings.filterwarnings("ignore", category=UserWarning)

# Define file paths for both spreadsheet reports
PARALLEL_CSV = "multi_dataset_scalability_report.csv"
# FIX: Unified filename spelling configuration layout
SEQUENTIAL_CSV = "multi_dataset_scalability_classic_report.csv"
OUTPUT_PNG = "nw_dual_asymptotic_scalability.png"
OUTPUT_PDF = "nw_dual_asymptotic_scalability.pdf"

# 1. Dataset Verification and Integrity Check
if not os.path.exists(PARALLEL_CSV) or not os.path.exists(SEQUENTIAL_CSV):
    print("❌ ERROR: Missing target spreadsheet data files.")
    print(f"Ensure both '{PARALLEL_CSV}' and '{SEQUENTIAL_CSV}' exist inside the local directory.")
    exit(1)

# Ingest both datasets cleanly
df_para = pd.read_csv(PARALLEL_CSV).sort_values(by="Total_Cells")
df_seq = pd.read_csv(SEQUENTIAL_CSV).sort_values(by="Total_Cells")

# Convert total cell matrix metrics into Billions for a clean X-axis typography layout
df_para['Total_Cells_Billions'] = df_para['Total_Cells'] / 1e9
df_seq['Total_Cells_Billions'] = df_seq['Total_Cells'] / 1e9

# Helper parsing engine converting float elements into clean Persian digits
def to_persian_digits(num_str):
    persian_map = {
        '0': '۰', '1': '۱', '2': '۲', '3': '۳', '4': '۴',
        '5': '۵', '6': '۶', '7': '۷', '8': '۸', '9': '۹', 
        '.': '/'  # Forward-slash separator safely maps on B Nazanin without blocks
    }
    return ''.join(persian_map.get(char, char) for char in str(num_str))

# 2. Configure Professional Academic Canvas & Global Font Engine
plt.figure(figsize=(10.5, 6.5))
sns.set_theme(style="whitegrid")

# Establish a fallback font chain to support cross-language string configurations
mpl.rcParams['font.sans-serif'] = ['B Nazanin', 'DejaVu Sans', 'Arial', 'Tahoma']
mpl.rcParams['font.family'] = 'sans-serif'

# 3. Plot Curve 1: Classic Single-Core Linear Baseline (Red Line)
plt.plot(
    df_seq['Total_Cells_Billions'], 
    df_seq['Wall_Time_Sec'], 
    color="#CB4335",           # Vibrant Coral Red
    linestyle="--",            # Dashed line style to denote baseline tracking
    linewidth=2.0, 
    marker="^",                # Triangle data markers
    markersize=6, 
    markerfacecolor="#CB4335",
    markeredgecolor="#7B241C",
    label="الگوریتم ترتیبی کلاسیک خط مبنا (یک هسته پردازشی)"
)

# 4. Plot Curve 2: Our Adaptive Parallel Method (Blue Line)
plt.plot(
    df_para['Total_Cells_Billions'], 
    df_para['Wall_Time_Sec'], 
    color="#1B4F72",           # Deep Navy Blue
    linestyle="-",             # Solid line style to denote our main contribution
    linewidth=2.5, 
    marker="o",                # Circle data markers
    markersize=7, 
    markerfacecolor="#1A5276",
    markeredgecolor="#1B4F72",
    label="روش پیشنهادی (تطبیقی با حافظه خطی بر روی ۸ هسته)"
)

# 5. Inject Exact Runtime Value Annotations for Key Milestone Nodes with Persian format
# Safe programmatic evaluation function preventing index faults on smaller trial files
total_rows_para = len(df_para)
total_rows_seq = len(df_seq)

if total_rows_para > 0 and total_rows_seq > 0:
    # Milestone 1: Label the 50K base data entry point (2.5 Billion cells)
    row_para_50 = df_para.iloc[0]
    row_seq_50 = df_seq.iloc[0]
    txt_seq_50 = to_persian_digits(f"{row_seq_50['Wall_Time_Sec']:.1f}") + " ثانیه"
    txt_para_50 = to_persian_digits(f"{row_para_50['Wall_Time_Sec']:.1f}") + " ثانیه"
    plt.annotate(txt_seq_50, (row_seq_50['Total_Cells_Billions'], row_seq_50['Wall_Time_Sec']), textcoords="offset points", xytext=(-15, 10), ha='center', fontsize=8, fontweight='bold', color="#7B241C", name='B Nazanin')
    plt.annotate(txt_para_50, (row_para_50['Total_Cells_Billions'], row_para_50['Wall_Time_Sec']), textcoords="offset points", xytext=(15, -15), ha='center', fontsize=8, fontweight='bold', color="#1B4F72", name='B Nazanin')

if total_rows_para >= 8 and total_rows_seq >= 8:
    # Milestone 2: Label the 150K base data entry point (22.5 Billion cells)
    row_para_150 = df_para.iloc[7]
    row_seq_150 = df_seq.iloc[7]
    txt_seq_150 = to_persian_digits(f"{row_seq_150['Wall_Time_Sec']:.1f}") + " ثانیه"
    txt_para_150 = to_persian_digits(f"{row_para_150['Wall_Time_Sec']:.1f}") + " ثانیه"
    plt.annotate(txt_seq_150, (row_seq_150['Total_Cells_Billions'], row_seq_150['Wall_Time_Sec']), textcoords="offset points", xytext=(-15, 10), ha='center', fontsize=8, fontweight='bold', color="#7B241C", name='B Nazanin')
    plt.annotate(txt_para_150, (row_para_150['Total_Cells_Billions'], row_para_150['Wall_Time_Sec']), textcoords="offset points", xytext=(15, -15), ha='center', fontsize=8, fontweight='bold', color="#1B4F72", name='B Nazanin')

if total_rows_para > 0 and total_rows_seq > 0:
    # Milestone 3: Label the absolute maximum 250K peak data nodes (62.5 Billion cells)
    row_para_250 = df_para.iloc[-1]
    row_seq_250 = df_seq.iloc[-1]
    txt_seq_250 = to_persian_digits(f"{row_seq_250['Wall_Time_Sec']:.1f}") + " ثانیه"
    txt_para_250 = to_persian_digits(f"{row_para_250['Wall_Time_Sec']:.1f}") + " ثانیه"
    plt.annotate(txt_seq_250, (row_seq_250['Total_Cells_Billions'], row_seq_250['Wall_Time_Sec']), textcoords="offset points", xytext=(-20, 10), ha='center', fontsize=8.5, fontweight='bold', color="#7B241C", name='B Nazanin')
    plt.annotate(txt_para_250, (row_para_250['Total_Cells_Billions'], row_para_250['Wall_Time_Sec']), textcoords="offset points", xytext=(20, -15), ha='center', fontsize=8.5, fontweight='bold', color="#1B4F72", name='B Nazanin')

# 6. Fine-Tune Typography and Axis Structural Bounds
plt.title("منحنی مقایسه مقیاس‌پذیری مجانبی زمان اجرای محاسبات\nروش پیشنهادی موازی تطبیقی در برابر مدل ترتیبی کلاسیک خط مبنا در فضای خطی", 
          fontsize=12, fontweight='bold', pad=15, color="#2C3E50", name='B Nazanin')

plt.xlabel("حجم کل ماتریس محاسباتی ترازسازی (میلیارد سلول)", fontsize=10, fontweight='bold', labelpad=10, color="#2C3E50", name='B Nazanin')
plt.ylabel("زمان اجرای واقعی محاسبات (ثانیه)", fontsize=10, fontweight='bold', labelpad=10, color="#2C3E50", name='B Nazanin')

# Apply automatic formatting overrides to convert tick markers to Persian layouts
ax = plt.gca()
ax.xaxis.set_major_formatter(plt.FuncFormatter(lambda x, pos: to_persian_digits(f"{x:.1f}")))
ax.yaxis.set_major_formatter(plt.FuncFormatter(lambda x, pos: to_persian_digits(f"{x:.0f}")))

# Run property updates to anchor bold styles onto font objects across ticks
for label in ax.get_xticklabels() + ax.get_yticklabels():
    label.set_fontname('B Nazanin')
    label.set_fontsize(10)
    label.set_fontweight('bold')

# Set exact axes boundary margins smoothly using valid bottom/top properties
plt.xlim(left=0, right=df_para['Total_Cells_Billions'].max() * 1.05)
plt.ylim(bottom=0, top=df_seq['Wall_Time_Sec'].max() * 1.10)

# Position legend layout with robust font formatting maps
plt.legend(loc='upper left', prop={'family': 'B Nazanin', 'size': 10, 'weight': 'bold'}, frameon=True, shadow=True, facecolor="#FDFEFE")

# Clean canvas boundary borders
sns.despine(left=True, bottom=True)
plt.tight_layout()

# 7. Export Print-Ready Media Objects
plt.savefig(OUTPUT_PNG, dpi=300)
plt.savefig(OUTPUT_PDF)

print("=========================================================================")
print(" 📈 DUAL-LINE SCALABILITY CURVE GENERATED SUCCESSFULLY! Figures saved:")
print(f"    ➡️ High-Resolution Image : {OUTPUT_PNG}")
print(f"    ➡️ Publication Vector PDF: {OUTPUT_PDF}")
print("=========================================================================")
