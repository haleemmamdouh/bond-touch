# Task 5: 3D Printing Enclosure Guide (Tinkercad & Slicer Settings)

Design your bracelet box in 5 minutes using Tinkercad (free in browser) and 3D print it.

---

## 1. Tinkercad Step-by-Step Drag & Drop Design Tutorial

1. Go to **[tinkercad.com](https://www.tinkercad.com)** (Sign in free with Google).
2. Click **Create New 3D Design**.
3. **Main Box Base:**
   - Drag a solid **Box** from the right shape library to the grid.
   - Set dimensions: Length = `54 mm`, Width = `28 mm`, Height = `18 mm`.
4. **Inner Hollow Cavity:**
   - Drag a **Hole Box** to the grid.
   - Set dimensions: Length = `50 mm`, Width = `24 mm`, Height = `16 mm`.
   - Lift it up `2 mm` off the floor using the black cone arrow.
   - Align both boxes to the center and click **Group** (Ctrl + G).
5. **USB Cable Slot:**
   - Drag a **Hole Box** (`12 mm` width, `8 mm` height).
   - Position it on the short wall and click **Group** to cut out the USB port hole.
6. **Removable Top Lid:**
   - Drag a solid Box (`54 mm` x `28 mm` x `2 mm` height).
   - Add an inner lip block (`49.5 mm` x `23.5 mm` x `2.5 mm` height) on top of it.
7. Click **Export** -> Download **.STL** file.

---

## 2. 3D Printer Slicer Settings (Cura / PrusaSlicer / OrcaSlicer)

| Setting Parameter | Recommended Value | Explanation |
|---|---|---|
| **Material** | **PLA** (Standard 1.75mm) | Easy to print, durable, no fumes |
| **Layer Height** | **0.2 mm** | Good balance of speed and detail |
| **Infill Density** | **20%** (Grid / Gyroid pattern) | Lightweight and strong |
| **Wall / Perimeter Count** | **3 Walls (1.2mm total)** | Ensures sturdy screwless snaps |
| **Print Speed** | **50 - 60 mm/s** | Standard printing speed |
| **Nozzle Temperature** | **200°C - 210°C** | Standard PLA temp |
| **Bed Temperature** | **60°C** | Prevents corner warping |
| **Supports** | **NONE (Disabled)** | Box is designed with flat overhang angles |
| **Est. Print Time** | **~45 - 60 minutes** per unit | Fast 3D print |
