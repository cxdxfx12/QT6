#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
快递运费计算脚本 - 基于ExpressFreightCalculator项目的默认报价表
使用pandas处理大数据量
"""

import pandas as pd
import numpy as np
import math
from collections import defaultdict

# ===================== 默认报价表数据 =====================

# 区域-省份映射
REGION_MAPPING = {
    "一区": ["江苏", "浙江", "安徽", "上海"],
    "二区": ["山东", "广东", "福建", "北京", "河南", "湖北", "湖南", "江西", "天津", "河北"],
    "三区": ["山西", "广西", "四川", "重庆", "陕西", "贵州", "辽宁", "吉林", "黑龙江", "云南"],
    "四区": ["海南", "甘肃", "青海", "内蒙古", "宁夏"],
    "五区": ["新疆", "西藏"]
}

def normalize_province(prov):
    """标准化省份名称"""
    if pd.isna(prov) or prov == "":
        return "未知"
    
    prov = str(prov).strip()
    
    # 去掉省、市后缀
    if prov.endswith("省") or prov.endswith("市"):
        prov = prov[:-1]
    elif prov == "内蒙古自治区":
        prov = "内蒙古"
    elif prov == "广西壮族自治区":
        prov = "广西"
    elif prov.startswith("新疆"):
        prov = "新疆"
    elif prov.startswith("西藏"):
        prov = "西藏"
    
    return prov

# 价格规则（按区域）
PRICE_RULES = {
    "一区": [
        {"min": 0, "max": 0.5, "price": 2.26, "type": "fixed"},
        {"min": 0.51, "max": 1, "price": 2.46, "type": "fixed"},
        {"min": 1, "max": 2, "price": 3.56, "type": "fixed"},
        {"min": 2, "max": 3, "price": 4.76, "type": "fixed"},
        {"min": 3, "max": 30, "first": 3.76, "add": 0.8, "type": "standard"},
        {"min": 30, "max": -1, "first": 3.86, "add": 0.8, "type": "standard"},
    ],
    "二区": [
        {"min": 0, "max": 0.5, "price": 2.26, "type": "fixed"},
        {"min": 0.51, "max": 1, "price": 2.46, "type": "fixed"},
        {"min": 1, "max": 2, "price": 3.56, "type": "fixed"},
        {"min": 2, "max": 3, "price": 4.76, "type": "fixed"},
        {"min": 3, "max": 30, "first": 3.76, "add": 1.1, "type": "standard"},
        {"min": 30, "max": -1, "first": 4.06, "add": 1.3, "type": "standard"},
    ],
    "三区": [
        {"min": 0, "max": 0.5, "price": 2.26, "type": "fixed"},
        {"min": 0.51, "max": 1, "price": 2.46, "type": "fixed"},
        {"min": 1, "max": 2, "price": 3.56, "type": "fixed"},
        {"min": 2, "max": 3, "price": 4.76, "type": "fixed"},
        {"min": 3, "max": 30, "first": 3.76, "add": 1.5, "type": "standard"},
        {"min": 30, "max": -1, "first": 4.06, "add": 1.6, "type": "standard"},
    ],
    "四区": [
        {"min": 0, "max": 0.5, "price": 2.56, "type": "fixed"},
        {"min": 0.51, "max": 1, "price": 3.56, "type": "fixed"},
        {"min": 1, "max": 2, "price": 4.06, "type": "fixed"},
        {"min": 2, "max": 3, "price": 5.06, "type": "fixed"},
        {"min": 3, "max": 30, "first": 3.76, "add": 2.5, "type": "standard"},
        {"min": 30, "max": -1, "first": 4.06, "add": 4.3, "type": "standard"},
    ],
    "新疆": [
        {"min": 0, "max": 0.5, "price": 10, "type": "fixed"},
        {"min": 0.51, "max": 1, "price": 13, "type": "fixed"},
        {"min": 1, "max": 2, "price": 20, "type": "fixed"},
        {"min": 2, "max": 3, "price": 25, "type": "fixed"},
        {"min": 3, "max": 30, "first": 15, "add": 15, "type": "standard"},
        {"min": 30, "max": -1, "first": 15, "add": 15, "type": "standard"},
    ],
    "西藏": [
        {"min": 0, "max": 0.5, "price": 13, "type": "fixed"},
        {"min": 0.51, "max": 1, "price": 15, "type": "fixed"},
        {"min": 1, "max": 2, "price": 25, "type": "fixed"},
        {"min": 2, "max": 3, "price": 30, "type": "fixed"},
        {"min": 3, "max": 30, "first": 15, "add": 15, "type": "standard"},
        {"min": 30, "max": -1, "first": 15, "add": 15, "type": "standard"},
    ],
}

def find_region(province):
    """根据省份查找区域"""
    prov = normalize_province(province)
    
    # 先检查新疆西藏（特殊价格）
    if prov in ["新疆", "西藏"]:
        return prov
    
    for region, provinces in REGION_MAPPING.items():
        if prov in provinces:
            return region
    
    # 默认返回一区
    return "一区"

def calculate_freight_single(weight, province):
    """计算单条运费"""
    if pd.isna(weight) or weight <= 0:
        return 0
    
    weight = float(weight)
    region = find_region(province)
    rules = PRICE_RULES.get(region, PRICE_RULES["一区"])
    
    for rule in rules:
        if weight > rule["min"] and (rule["max"] == -1 or weight <= rule["max"]):
            if rule["type"] == "fixed":
                return rule["price"]
            else:
                # 标准计算：首重 + 续重（续重按比例，不取整，与C++一致）
                if weight <= rule["min"]:
                    return rule["first"]
                extra_weight = weight - rule["min"]
                if extra_weight < 0:
                    extra_weight = 0
                return rule["first"] + extra_weight * rule["add"]
    
    return 0

# 向量化计算运费
def calculate_freight_vectorized(weights, provinces):
    """批量计算运费"""
    freights = []
    regions = []
    
    for i in range(len(weights)):
        w = weights[i]
        p = provinces[i]
        r = find_region(p)
        f = calculate_freight_single(w, p)
        freights.append(f)
        regions.append(r)
    
    return freights, regions

def process_excel(input_file, output_file):
    """处理Excel文件"""
    print(f"正在读取: {input_file}")
    
    # 读取Excel
    df = pd.read_excel(input_file, engine='openpyxl')
    
    print(f"数据总行数: {len(df)}")
    print(f"列名: {df.columns.tolist()}")
    
    # 确保必要列存在
    required_cols = ["业务时间", "结算重量", "目的省份", "订单客户", "客户"]
    for col in required_cols:
        if col not in df.columns:
            print(f"警告: 缺少列 {col}")
    
    # 计算运费
    print("开始计算运费...")
    
    weights = df["结算重量"].values if "结算重量" in df.columns else [0] * len(df)
    provinces = df["目的省份"].values if "目的省份" in df.columns else [""] * len(df)
    
    freights, regions = calculate_freight_vectorized(weights, provinces)
    
    # 添加计算结果列
    df["计算重量(kg)"] = df["结算重量"]
    df["运费(元)"] = freights
    df["区域"] = regions
    
    # 统计
    total_count = len(df)
    total_freight = sum(freights)
    error_count = sum(1 for f, w in zip(freights, weights) if f == 0 and not pd.isna(w) and w > 0)
    
    print(f"\n===== 处理完成 =====")
    print(f"总条数: {total_count}")
    print(f"总运费: {total_freight:.2f} 元")
    print(f"平均运费: {total_freight / total_count:.2f} 元")
    print(f"错误条数: {error_count}")
    
    # 各区域统计
    region_stats = df.groupby("区域").agg({
        "运费(元)": ["count", "sum"]
    }).reset_index()
    region_stats.columns = ["区域", "条数", "总运费"]
    region_stats["平均运费"] = region_stats["总运费"] / region_stats["条数"]
    region_stats = region_stats.sort_values("总运费", ascending=False)
    
    print("\n各区域统计:")
    for _, row in region_stats.iterrows():
        print(f"  {row['区域']}: {row['条数']}条, 总运费 {row['总运费']:.2f}元, 平均 {row['平均运费']:.2f}元/件")
    
    # 按省份统计
    province_stats = df.groupby(df["目的省份"].apply(normalize_province)).agg({
        "运费(元)": ["count", "sum"]
    }).reset_index()
    province_stats.columns = ["省份", "条数", "总运费"]
    province_stats["平均运费"] = province_stats["总运费"] / province_stats["条数"]
    province_stats = province_stats.sort_values("总运费", ascending=False).head(20)
    
    print("\nTop 20 省份统计:")
    for _, row in province_stats.iterrows():
        print(f"  {row['省份']}: {row['条数']}条, 总运费 {row['总运费']:.2f}元, 平均 {row['平均运费']:.2f}元/件")
    
    # 保存结果
    print(f"\n正在保存结果到: {output_file}")
    
    # 保存主表
    output_cols = ["业务时间", "运单号", "结算重量", "目的省份", "体积重", 
                   "订单客户", "客户", "计算重量(kg)", "运费(元)", "区域"]
    output_df = df[[c for c in output_cols if c in df.columns]]
    
    with pd.ExcelWriter(output_file, engine='openpyxl') as writer:
        output_df.to_excel(writer, sheet_name="运费计算结果", index=False)
        
        # 添加汇总表
        summary_data = {
            "统计项": ["总条数", "总运费(元)", "平均运费(元)", "错误条数"],
            "数值": [total_count, round(total_freight, 2), round(total_freight/total_count, 2), error_count]
        }
        summary_df = pd.DataFrame(summary_data)
        summary_df.to_excel(writer, sheet_name="汇总", index=False)
        
        # 区域统计表
        region_stats.to_excel(writer, sheet_name="区域统计", index=False)
        
        # 省份统计表（全部）
        all_province_stats = df.groupby(df["目的省份"].apply(normalize_province)).agg({
            "运费(元)": ["count", "sum"]
        }).reset_index()
        all_province_stats.columns = ["省份", "条数", "总运费"]
        all_province_stats["平均运费"] = all_province_stats["总运费"] / all_province_stats["条数"]
        all_province_stats = all_province_stats.sort_values("总运费", ascending=False)
        all_province_stats.to_excel(writer, sheet_name="省份统计", index=False)
    
    print(f"结果已保存!")
    
    return {
        "total_count": total_count,
        "total_freight": total_freight,
        "region_stats": region_stats,
        "province_stats": province_stats
    }

if __name__ == "__main__":
    input_file = "蜜丝婷-4月发件账单表1.xlsx"
    output_file = "蜜丝婷-4月运费计算结果.xlsx"
    result = process_excel(input_file, output_file)