# Widescreen per scene — 03/10/2026

The 51 saves provided were loaded without changing the originals. Selectors below: word $0400 (phase index at $0401), script $1126.

|Scene|Selector / script|Rule|
|---|---|---|
|Banpresto|0000 / 9447, heading $1124 until 0037|Full background|
|Opening after the logo|0000 / 9447, rear position|4:3|
|Title|0000 / BB63|Wrapping|
|Dialogue before the first phase|0000 / A483, stride zero|4:3|
|Stage 1 phase 2|0200 / A483|Real Map and Limits|
|Stage 2 phase 3, up/down|0A00, 0B00 / AA11|Strides 2000, 1800|
|Stage 2 phase 5, low|1000 / AA11|Stride 1000|
|Boss of Stage 2|1100 / AA11|4:3 fixed|
|Stage 3 phase 5|1600 / A8A6|Stride 0800|
|Stage 4 dialogue|1900 / A718|Real room limits|
|Boss of Stage 4|2400 / A718|Stride 0800 and real limits|
|Car road|2500 / A56E|Repeat VRAM after HDMA per line|
|Stage 5 phase 1|2600 / A56E|Stride 1200|
|Stage 5 phases 3/4|2A00, 2B00 / A22E|Strides 2C00, 1C00|
|Stage 5 phase 3 story (castle)|2800 / A1D9|Native composition, pillarbox and original letterbox; aspect 4:3 or 8:7|
|Final score|2D00 / 98B4|Wrapping, Start → fade → boot|

The $80:B028 table leads to the camera regions used in $80:A8E0. Limits are inclusive to the right: right margin = limit + 1 - camera - 256. The left margin = camera - left limit. Both are limited by the valid buffer on WRAM And for the 64 pixel budget on each side. Smaller rooms do not gain a nonexistent scenario: the extension outside of them becomes black. The selector $0200 is no longer used alone as an exception to history.

On the road, the VRAM It's lively and the scroll changes by HDMA. They are designed at most eight tiles of the current line after replay; everything is restored before the next line. The central region was compared with a capture at 4:3 and is identical. The remaining 50 central regions are identical to the previous build samples.

## Reproducible validation

- CTest: seven suites, including map limits, 1024 offsets of road scroll, preservation of VRAM, fade of 60 frames and cancellation after load.
- scripts/validate_scenes.py: 51 saves × 1000 frames, prep in frame 10, comparison RAM/VRAM/OAM/SPC, central pixels, masks and cadence of 357368 clocks between frame boundarys. Use private fixtures, not distributed.
- scripts/smoke.py: 1000 frames, requires termination and absence of watchdog/timeouts.
- Score: frame 73 completely black, reset in frame 74, execution up to 1000.
- Private memory monitored after rewind warming. This detects persistent growth in the test, but does not prove absence of all leakage and does not replace ASan/LeakSanitizer. The local compiler doesn't offer that runtime.

Headless tests do not measure physical tearing, device latency or hearing quality. The final gameplay test remains useful for these points.

## Update DisplayAspect — 10/10/2026

The 64-pixel historical ceiling per side described above was replaced by 112. The width now derives from PAR selected and rounded up into even columns: window 1344x672 receives 384x224 in CRT and 448x224 in Square Pixels. The viewport uses uniform scale to cover the window, cutting only the rounding surplus. The original mode remains centralized with bars.

112 pixels equal seven 16px tiles per side. The 32 column ring retains up to 17 native columns plus 14 extras, leaving a free column. Increasing this ceiling without a separate rendering tilemap would cause aliasing on the native image. A PPU It has greater capacity, but it doesn't make the game's tilemap bigger. The road's temporary buffer has been extended from eight to fourteen entrances; it continues restoring VRAM after each line and respecting scroll HDMA current. Checks from WRAM, camera regions, map boundaries, scene masks and HUD Natives are still active. Black due to nonexistent scenario remains intentional.

In ultra-large proposals that ask for more than 480 internal pixels, the viewport fills the window with centered vertical clipping, preserving the PAR And the limits of memory. This could cut off some of the HUD; does not represent unlimited expansion.

Validation candidate in private/aspect-fill-candidate/: 14/14 CTests; 51 saves in both aspects (102 combinations), comparison of RAM/VRAM/OAM/SPC and central pixels in frame 10; smoke of 1000 frames in each aspect. The scenes were performed by 12 frames in this image regression, not for 1000 each.

## Double Moon in Save GGA1_save_01 — 10/10/2026

The root save plays selector $2A00, script $A22E, mode $39 and BG2SC$54. O BG2 repeats the art every 16 columns of 16px: the quad of the moon in the lines 2/3, columns 11/12, reappears in the columns 27/28. Words are $04A0, $04A2, $04A4, $04A6; the $10A8 sky tile provides moonless filling.

The exception of the sky compares scene, layout and complete signature of each quad. After the replay raster/HDMA, use hScroll[1] real to preserve any instance that intersects [0,256]. Only entirely outer quads receive temporary sky; the ring and clouds continue to expand normally. The words VRAM are restored at the end of each scanline. The exception now is worth so much to CRT as for Square Pixels; original mode, another scene or changed signature do not activate the exception.

Validation in private/moon-wrap-candidate/: original saved copied without change; captures before/after in both aspects, 413 pixels changed only in the rectangle x=17..47, y=32..63 of the outer moon. Yellow pixel count confirms your removal. Central region, whole image CRT and RAM/VRAM/OAM/SPC in frame 10 are identical. The 14 CTests pass, including 512 alternating scroll phases PAR, neighboring scene protection and different signature. Save smoke loaded: 1000 frames per aspect. Automated validation; there was no manual alternation via physical keyboard during this round.

## Scene policy lock during fades — 10/10/2026

Premature change happens before the brightness drop: the logo script position changes the policy in frame 350, with INIDISP still $0F; the black frame arrives only in 397. When giving Start in the first phase story save, the script changes $A483 for $A565 In frame 34, but the image only erases in frame 63. Detecting only decreasing brightness does not protect this initial window.

AckmanFlowVideoPolicy maintain the scene policy from the first visible frame to AckmanFlowVideoEndpoint confirm the entirely black composite image. AckmanVideoEnd check RGB after rendering, masks and software fade used in the final punctuation. Force Blank alone and brightness do not constitute proof of endpoint: rasterization can change INIDISP inside the frame. The first totally black frame releases the policy into the next frame; during the fade-in, the policy of fate remains maintained. The lock is restarted by reboot and changes in the state generation (load/rewind), without serialization in the save.

Only the choice of policy (wrap, native, map or road) is blocked. The valid limits of maps and scroll remain recalculated to preserve security; the preference of widescreen and DisplayAspect The user is still separated. No clock, log or guest brightness routine has been altered.

ACKMAN_FADE_TRACE export policy requested/effective, margins and endpoint RGB by frame; ram_0270 and a crude sample, not a measure of brightness. Evidence in private/fade-safe-candidate/: comparison before/after of logo and start of history in CRT and Square Pixels, 1000 frames per case; every change of effective policy requires the previous frame to be black. Soon he only swaps in 398; history in
64. RAM/VRAM/OAM/SPC and central sample pixels (375/45) remain identical. 14/14 CTests, including both-way latch, reset by load and reboot, Force Blank without black image and black endpoint without depending on INIDISP. This is automated coverage of the reproduced transitions, not manual inspection of all the possible scenes of the game.

## Story geometry and closure saves — 10/10/2026

Saves provided in saves/ were copied for private fixtures, loaded and sampled in frame 10. These rules replace the old expansion of history ABD9/A964 only on the indicated selectors:

|Save/scene|Selector $0400|Script $1126|Policy|
|---|---|---|---|
|Stage 2 story|0600|ABD9|Native/pillarbox|
|Stage 3 story|1100|A964|Native/pillarbox|
|Stage 1 Ackman and Angel (reference)|0000|A34D|Only BG2 expanded|
|Stage 2 Ackman and Angel|0600|A34D|Only BG2 expanded|
|Stage 3 Ackman and Angel|1100|A34D|Only BG2 expanded|
|Stage 4 Ackman and Angel|1800|A34D|Only BG2 expanded|
|Stage 5 Ackman and Angel|2400|A34D|Only BG2 expanded|
|End|2D00|9A4A|Expanded BG2 sky/scenery|
|Final credits|2D00|AC82|BG1+BG2 Periodics; central text|
|End THE END|2D00|B523|Track BG1 expanded on lines 64..143|

The canvas remains stable not to break fades; the native area and presented with the PAR of DisplayAspect, therefore pillarbox accompanies 4:3 CRT or 8:7 Square Pixels. The full policy, including which layers extend, goes through the latch of AckmanFlow: changes only enter after a black frame or reset/load.

A34D use BG2 As a background pattern Ackman. BG1 contains the stage title and the blue box; BG3 and text/HUD. BG1/BG3/OBJ are restricted to [0,256]. The repetition uses the native scanout isolated from BG2Instead of picking up tiles outside the initialized area.

In the credits, BG1/BG2 provide the periodic landscape; BG3 and OBJ They're native. In the end, the sky BG2 can use the initialized map and receives the same protection from the outer moon in Square Pixels, conditioned the signature of the quad. THE END expands only the background of the blue track (screen y=63..142); letters and characters do not repeat. Black above/below of this track and original art, not pillarbox.

The playable patch patches/ackman-obj-clamp.patch adds Clamp bit 4 to limit only the composition of OBJ, both main and subscreen. OAM, evaluation of sprites, windows and native pixels do not change. CMake checks/applies That patch next to the previous ones. The bits BG0..BG3 and APIs existent remain compatible.

Evidence: private/scene-layout-candidate/. 14/14 CTests; 10 saves including the reference, in two aspects: 20 cases x 1000 frames. Captures compared in frame 10 prove RAM/VRAM/OAM/SPC and native identity center; background margins are equal to captures with only permitted layers; repetition is equal to samples of the native area and pillarbox and entirely black. The range of THE END was validated separately from the letters. Original Fixtures and config.ini root has not been modified. Automated clipping validation without manual testing.


## Ending corrections from additional gameplay saves — 10/10/2026

All new rules require selector `$2D00`; they do not enable global dialogue widescreen. The additional saves identify these compositions:

|Save/sequence|Script|Layer policy|
|---|---|---|
|final credits 02|B523, BGMODE 79|Repeat initialized native BG1 **and BG2**; keep BG3 and OBJ central|
|final / castle entrance|9A4A|Expand BG2 and OBJ; keep BG1/BG3 central|
|Castle continuation|9AB9, 9AF0, 9B2F, 9BDC, 9BE4|Same castle composition|
|final 02 / sky dialogue|9D0D|Expand BG2; keep BG1/BG3/OBJ central|
|Following mountain dialogue|9F08|Repeat BG1/BG2; keep BG3/OBJ central|
|Credits|AC82|Same two-background landscape policy|
|THE END|B523, BGMODE 39|Preserve the original BG1 band-only expansion, lines 64..143|

B523 is a shared script, not an identifier exclusive to THE END. Its landscape phase uses BGMODE 79 (large) BG3 tiles); THE END Use 39. The full layer policy still goes through AckmanFlow's black-frame latch, so this distinction cannot change the presentation halfway through a visible fade.

The castle truncation was caused by the wrapper's OBJ composition clamp (`0x15`), not a missing guest OAM object. The castle policy now uses `0x05`, allowing the existing PPU extended OAM evaluation/composition bounds to show its off-center pieces. It doesn't change guest OAM, sprite limits or bounds for other scene policies. Dialogue portraits continue to use the OBJ Clamp.

9D0D uses the moon sky signature in BGMODE 79. The signature-checked, render-only exterior moon suppression therefore accepts both 39 and 79, restoring every touched VRAM word after the scan as before.

Reproduction: `scripts/validate_ending_fixes.py --exe build-validation/AckmanRecomp.exe --rom <ROM>`. Evidence is written to `private/ending-fixes-validation/`: four saves (the three reported cases plus THE END regression), both 4:3 and 8:7, 1000 frames each. Frame-10 comparisons check exact native-center pixels and RAM/VRAM/OAM/SPC equality against an unextended render. Isolated layer captures verify landscape-only borders and current castle OBJ pixels beyond native x=256. Longer input replays exercise 9D0D → 9F08 → AC82 and B523 landscape → THE END, including black endpoints. These are automated SDL/PPU captures, not a physical keyboard/gamepad session. The original saves and root config.ini Remain untouched.

Candidate result: 14/14 CTests passed; the additional BGMODE-79 moon regression passed. All ending replay checks above passed, along with the standalone 1000-frame smoke test. Root AckmanRecomp.exe SHA256: C3060BE8C18E4F6DE992A4FDA1498404448EB2F2FA2315E64183780634696C07. Previous executable: private/pre-ending-fixes/AckmanRecomp.exe.

## Universal moon exception — 10/10/2026

The sky exception no longer depends on DisplayAspect. Scene/script guards, full tile signatures, native-image intersection checks and scanline VRAM restoration remains unchanged. This also protects ending and epilogue skies.

The current saves/GGA1_save_01.sav Identifiers selector 2D00 / script 9D0D. Private before/after frame-10 captures show 309 changed pixels exclusively in the external duplicate moon at x=0..15, y=31..62 in CRT. Square Pixels is pixel-identical to the previous build. Both native centers and guests WRAM/VRAM/OAM/SPC snapshots are unchanged; the source save is unchanged.

Validation: all 14 CTests passed, including every 512 scroll phases in both aspects and the BGMODE-79 epilogue regression. Save replays completed 1000 frames in each aspect. Evidence: private/universal-moon-candidate/.
