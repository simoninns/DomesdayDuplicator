# Overview

Important: If you are looking for information on how to perform disc stacking using the ld-decode tool-chain (recommended unless you know what you are doing) please use the following link rather than this article:

[Working with multiple discs](../How-to-guides/Working-with-multiple-discs.md)

The following is a guide on how to use the [vapoursynth-median](https://github.com/dubhater/vapoursynth-median) plugin for [Vapoursynth](http://www.vapoursynth.com/) to reduce noise and dropouts and improve detail by using multiple copies of a disc. It requires a working vapoursynth installation, the plugin, and [vsedit](https://bitbucket.org/mystery_keeper/vapoursynth-editor).

Initial NTSC results with a 3 sources and 5 sources show substantial improvement in SNR, a great reduction in dropouts, and on the 5-source median notably stronger chroma.

# Preparation

In order to work, a minimum of three copies of a disc is required. At the time of writing the median plugin can only make use of an odd number of copies. If for example there's four copies available, the best three should be used, or a fifth copy should be found. Additionally, not just any copies will do- in order to work at the TBC level they must all be printed from the same master plate. (It may be possible to use video from different masters after using ld-chroma-decoder, however, in a modified process.)

To determine if disc copies match or not, the mint marks near the spindle of the disc can be checked. Discs will generally have two markings- the plastic batch number and master reference. The plastic batch number generally looks solidly engraved in, and is of no importance for this. The master mint mark indicates which master plate the pressing came from and can appear quite different across manufacturers. It can appear printed, engraved, or scrawled in by hand. The two marks can be anywhere along the spindle, sometimes even printed over each other, making the master mark hard to read.

In this image, the plastic batch is on top, and the master mark is below it. The "A09" means it is the 9th master plate for side A, and to median stack more copies from plate A09 would be needed.

![Mint marks](assets/mintmarks1.jpg "Both Mint marks")

Here's an example from a scratched-in mark. It's nearly impossible to read even in person, but appears to contain A-2, 2nd master plate for side A.

![Illegible mark](assets/mintmarks2.jpg "Illegible master mark")

Note: Because discs are made by gluing two sides together, two discs may have matching masters on one side, but differ on the other.

Decoding:

The normal procedure for decoding a disc should be followed, making sure that each decode begins from the same frame number. ld-dropout-correct should be run on each .tbc, as median-stacking will not correct dropouts that originate from the master plate itself.
Even corrected, dropouts unique to each disc should not be visible after median stacking, so there is little harm in correcting them all at this stage.

# Median Stacking

Edit and save the following script, filling in the paths to each dropout-corrected TBC, uncommenting or adding any beyond the initial 3, then open it with vsedit.

medianstack.vpy:

```
import vapoursynth as vs
import mvsfunc as mvs
core = vs.get_core()

#Load raw .tbc files. Should be dropout-corrected to catch any dropouts from the master plate. NOTE: For PAL, set width to 1135 and height to 313, and fpsnum=50000, fpsden=1000
a = core.raws.Source(r'side1-a.doc.tbc', width=910, height=263, src_fmt="gray16", fpsnum=60000)
b = core.raws.Source(r'side1-b.doc.tbc', width=910, height=263, src_fmt="gray16", fpsnum=60000)
c = core.raws.Source(r'side1-c.doc.tbc', width=910, height=263, src_fmt="gray16", fpsnum=60000)
#d = core.raws.Source(r'side1-d.doc.tbc', width=910, height=263, src_fmt="gray16", fpsnum=60000)
#e = core.raws.Source(r'side1-e.doc.tbc', width=910, height=263, src_fmt="gray16", fpsnum=60000)
#f = core.raws.Source(r'side1-f.doc.tbc', width=910, height=263, src_fmt="gray16", fpsnum=60000)
#g = core.raws.Source(r'side1-g.doc.tbc', width=910, height=263, src_fmt="gray16", fpsnum=60000)


# Check number of fields for each .tbc in the list. Printed to console.
print("a:", a.num_frames, "fields")
print("b:", b.num_frames, "fields")
print("c:", c.num_frames, "fields")
#print("d:", d.num_frames, "fields")
#print("e:", e.num_frames, "fields")
#print("f:", f.num_frames, "fields")
#print("g:", g.num_frames, "fields")


"""
#Match length to reference .tbc.
#If one or more has a stray field in the beginning, should start from [1:X.num_frames] to align to the shortest. Use the shortest's json for the median output.
a = a[0:a.num_frames]
b = b[0:b.num_frames]
c = c[0:c.num_frames]
#d = d[0:d.num_frames]
#e = e[0:e.num_frames]
#f = f[0:f.num_frames]
#g = g[0:g.num_frames]
"""


#Median stack captures together. Requires odd number of sources.
median = core.median.Median([a, b, c], sync=0, debug=False) # Add any additional sources to the list. Change sync=0 to sync=2 or higher search radius to look for matching fields if the TBCs are not perfectly aligned. Multiples of 2 are probably best. Processing will be slower the higher this is set.


#Preview as full frames
frameout = median # Set to which TBC you wish to view, or median.
frameout = core.std.DoubleWeave(frameout, tff=1)
frameout = core.std.SelectEvery(frameout, 2, 0)
#frameout = core.text.FrameNum(frameout, 1) # Overlay current frame number, good for referencing ld-analyse when previewing IVTC. (add 1)

#Inverse telecine preview
gray8 = core.resize.Bilinear(frameout, format=vs.GRAY8) # VIVTC only supports gray8, so adjust depth for preview.
ivtc = core.vivtc.VFM(gray8,1,field=0) #field-match - top field first, prefer keeping bottom field.
ivtc = core.vivtc.VDecimate(ivtc)

#Be sure to set to median for .tbc output. Otherwise set to frameout or ivtc for previewing in vsedit.
median.set_output()
```

The first step is to verify that each .tbc in fact contains the same number of fields.
Uncomment the clips you've loaded in the field checking block, and run Preview or Check Script.
The number of fields for each TBC will be printed to the console. If they all match, nothing further needs to be done.

If the number of fields do not match, alignment may need to be done.
By changing the last line in the script to `a.set_output()`, `b.set_output()`, and so on, check the preview to see how each starts. If there's a stray lead-in field at the beginning of one or more, it can be trimmed by uncommenting the match length section, and adjusting the start frame accordingly. All should begin from the same first field. Field/frame numbers start from 0 in vapoursynth.

If the numbers still do not match after alignment, some of the TBCs may have missing or extra fields. the median filter can try to search for the matching fields by setting the sync parameter to the range to search for.

Once aligned, you can preview how the result will look by changing the bottom line to `frameout.set_output()` and pressing F5 or selecting Preview from the Script menu. It will show frame output similar to ld-analyse. The number of frames should now match the reference TBC as viewed in ld-analyse. Note that the frame numbers will be off by 1, due to vapoursynth counting from 0. If your source is NTSC telecined material, you can also preview reverse pulldown by changing the bottom line to `ivtc.set_output()`
You can change the preview from the median to any individual tbc by changing the line `frameout = median` to `frameout = a` or the one you wish to view. 


ld-analyse reference tbc, vsedit median preview:

![vsedit preview](assets/vsedit_preview.png "vsedit preview")

![ld-analyse preview](assets/ld-analyse.png "ld-analyse preview")

Once satisfied, set the bottom line to median.set_output(), save, and close vsedit.

The median TBC can now be saved with `vspipe medianstack.vpy output.tbc`
Pair with a copy of the chosen reference TBC's json and proceed with normal processing.

