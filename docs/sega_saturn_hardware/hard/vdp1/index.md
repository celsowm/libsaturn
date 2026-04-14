# VDP1 User's Manual/Table of Contents

[Japanese](javascript:window.location.href = window.location.pathname.replace('.htm', '_j.htm');)

---

[![]()](https://docs.exodusemulator.com/Archives/SSDDV25/segahtml/index.htm) ★ [HARDWARE Manual](../index.md)

---

# VDP1 User's Manual

## 1st edition/Rel.2

---

### Introduction : This manual explains the functions of VDP1 and how to use it. VDP1 mainly defines drawing data and draws.

### Manual structure : This manual consists of the following chapters, [a table of contents with figures](zumokuzi.md) , and [an index](sakuin.md) .

### [Chapter 1 Functions of VDP1](hon/p01_10.md) : **[■1.1 What is VDP1?](hon/p01_10.md)**: **[●System configuration](hon/p01_10.md)**: **[●Functions of VDP1](hon/p01_10.md)**: **[■1.2 Screen mode](hon/p01_20.md)**: **[●Screen mode and display range](hon/p01_20.md)**: **[●Frame buffer rotation readout](hon/p01_20.md)** ### [Chapter 2 Address Map](hon/p02_10.md) : **[■2.1 Address map](hon/p02_10.md)**: **[●Frame buffer](hon/p02_11.md)**: **[●System register](hon/p02_12.md)**: **[■2.2 Table on VRAM](hon/p02_20.md)** ### [Chapter 3 Process flow](hon/p03_10.md) : **[●Flow of drawing procedure](hon/p03_10.md)**: **[●Command table flow](hon/p03_10.md)**: **[●Table reference](hon/p03_10.md)** ### [Chapter 4 System Register](hon/p04_10.md) : **[●System register list](hon/p04_10.md)**: **[■4.1 TV mode selection register](hon/p04_11.md)**: **[■4.2 Frame buffer switching mode register](hon/p04_12.md)**: **[●Sequence when using erase & change](hon/p04_12.md)**: **[●Usage example](hon/p04_12.md)**: **[■4.3 Plot trigger register](hon/p04_13.md)**: **[■4.4 Erase write](hon/p04_14.md)**: **[■Erase write data register](hon/p04_14.md)**: **[■Erase write upper left coordinate register](hon/p04_14.md)**: **[■Erase write lower right coordinate register](hon/p04_14.md)**: **[■4.5 Forced drawing termination register](hon/p04_15.md)**: **[■4.6 Transfer end status register](hon/p04_16.md)**: **[■4.7 Processing suspension table address register](hon/p04_17.md)**: **[■4.8 Current processing table address register](hon/p04_18.md)**: **[■4.9 Mode status register](hon/p04_19.md)** ### [Chapter 5 Table](hon/p05_10.md) : **[■5.1 Character pattern table](hon/p05_10.md)**: **[■5.2 Color lookup table](hon/p05_20.md)**: **[●Lookup table mode](hon/p05_20.md)**: **[●Character pattern](hon/p05_20.md)**: **[●Command table](hon/p05_20.md)**: **[■5.3 Gouraud shading table](hon/p05_30.md)**: **[●Gouraud shading](hon/p05_30.md)**: **[●Specifying Gouraud shading](hon/p05_30.md)**: **[●Gouraud shading processing](hon/p05_30.md)**: **[■5.4 Command table](hon/p05_40.md)** ### [Chapter 6 Command Table](hon/p06_10.md) : **[■6.1 CMDCTRL (control word)](hon/p06_10.md)**: **[■6.2 CMDLINK (LINK specification)](hon/p06_20.md)**: **[■6.3 CMDPMOD (drawing mode word)](hon/p06_30.md)**: **[■High speed shrink](hon/p06_31.md)**: **[■Pre-clipping disabled](hon/p06_32.md)**: **[■User clipping enabled](hon/p06_32.md)**: **[■User clipping mode](hon/p06_32.md)**: **[■Mesh enabled](hon/p06_33.md)**: **[■End code invalid](hon/p06_34.md)**: **[■Transparent pixels disabled](hon/p06_35.md)**: **[■Color mode](hon/p06_36.md)**: **[■Color calculation](hon/p06_37.md)**: **[■MSB on](hon/p06_38.md)**: **[■6.4 CMDCOLR (color control word)](hon/p06_40.md)**: **[■Color bank](hon/p06_40.md)**     **[■Color lookup table](hon/p06_40.md)**: **[■Non-texture color](hon/p06_40.md)**: **[■6.5 CMDSRCA (character address)](hon/p06_50.md)**: **[■6.6 CMDSIZE (character size)](hon/p06_60.md)**: **[■6.7 CMDXA~CMDYD (vertex coordinate data)](hon/p06_70.md)**: **[■6.8 CMDGRDA (Gouraud shading table)](hon/p06_80.md)** ### [Chapter 7 Commands](hon/p07_00.md) : **[■7.1 System clipping coordinate setting command](hon/p07_10.md)**: **[●System clipping](hon/p07_10.md)**: **[■7.2 User clipping coordinate setting command](hon/p07_20.md)**: **[●User clipping](hon/p07_20.md)**: **[■7.3 Relative coordinate setting command](hon/p07_30.md)**: **[●Relative coordinates](hon/p07_30.md)**: **[■7.4 Standard sprite drawing commands](hon/p07_40.md)**: **[■7.5 Rectangular sprite drawing command](hon/p07_50.md)**: **[●Specify two coordinates (rectangular sprite drawing command)](hon/p07_50.md)**: **[●Fixed point specification (rectangular sprite drawing command)](hon/p07_51.md)**: **[■7.6 Transformed sprite drawing command](hon/p07_60.md)**: **[■7.7 Polygon drawing commands](hon/p07_70.md)**: **[■7.8 Polyline drawing command](hon/p07_80.md)**: **[■7.9 Line drawing commands](hon/p07_90.md)**: **[■7.10 Drawing end command](hon/p07_a0.md)** ### [Chapter 8 Quick Reference](hon/p08_10.md) ### [Chapter 9 Precautions for use](hon/p09_10.md) : **[●VDP1 general](hon/p09_10.md)**: **[●System register](hon/p09_10.md)**: **[●Command](hon/p09_10.md)** ### [explanation of words](hon/p90_10.md) ### [Table of contents](zumokuzi.md) ### [index](sakuin.md)

---

[![]()](https://docs.exodusemulator.com/Archives/SSDDV25/segahtml/index.htm) ★ [HARDWARE Manual](../index.md)

---

 Copyright SEGA ENTERPRISES, LTD., 1997
