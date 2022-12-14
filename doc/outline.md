# Outline #

Following is a general outline of the game, starting at the entry point.

The tree omits descendant functions which do not contribute to the 'game flow'.

```
- main - main.c
  - init - psx/init.c
    - CdInit - psx/cdr.c
    - PadInit - pad.c
    - CardInit - psx/card.c
    - GpuInit - psx/gpu.c
    - CdReadFileSys - psx/cdr.c
  - CoreLoop - main.c
    - NSInit - ns.c
      - GpuSetupPrims - psx/gpu.c
      - SlstInit - slst.c
      - BinfInit - solid.c
      - GoolInitAllocTable - gool.c
      - AudioInit - audio.c
      - MidiInit - midi.c
      - TitleInit - title.c
      - PbakInit - pbak.c
    - CoreObjectsCreate - main.c
      - GoolObjectCreate - gool.c
    - main loop
      - detect press start/handle pause screen
      - handle demo playback if there is a pbak entry
        - PbakPlay - pbak.c
      - handle changes in current level; if there is a change then
        - GoolSendToColliders - gool.c // kill all objects
        - NSKill - ns.c // unload current level
        - NSInit - ns.c // load next level
        - CoreObjectsCreate - main.c // recreate core objects
        - handle returns from bonus round
          - LevelSpawnObjects - level.c
          - LevelRestart - level.c
      - LevelSpawnObjects - level.c // spawn all objects
      - update shader params if not paused
        - ShaderParamsUpdate - misc.c
        - ShaderParamsUpdateRipple - misc.c
      - CamUpdate - cam.c
        - CamFollow - cam.c
          - CamGetProgress - cam.c
          - CamAdjustProgress - cam.c
            - LevelUpdate - level.c
              - SlstUpdate - slst.c
                - RSlstDecodeForward - psx/r3000a.s
                - RSlstDecodeBackward - psx/r3000a.s
              - ZoneTerminateDifference - level.c
              - NSZoneUnload - ns.c
              - NSOpen - ns.c
              - NSPageOpen - ns.c
              - LevelUpdateMisc - level.c
              - ZonePathProgressToLoc - level.c
      - GfxUpdateMatrices - gfx.c
        - GfxLoadWorlds - gfx.c
      - GfxTransformWorlds* - gfx.c
        - RGteTransformWorlds* - psx/r3000a.s
      - GoolUpdateObjects - gool.c
        - GoolObjectUpdate - gool.c
          - PadUpdate - pad.c
          - GoolObjectBound - gool.c
            - GoolObjectLocalBound - gool.c
            - GoolTransform - gool.c
            - GoolCollide - gool.c
          - GoolObjectInterpret - gool.c
          - GoolObjectColors - gool.c
          - GoolObjectPhysics - gool.c
            - GoolObjectControlDir - gool.c
            - GoolObjectOrientOnPath - gool.c
            - TransSmoothStopAtSolid - solid.c
              - TransPullStopAtSolid - solid.c
                - TransStopAtSolid - solid.c
                  - StopAtFloor - solid.c
                    - ZoneQueryOctrees - level.c
                    - FindFloorY - solid.c
                    - StopAtHighestObjectBelow - solid.c
                      - GoolCollide - gool.c
                  - PlotWalls - solid.c
                    - PlotQueryWalls - solid.c
                      - PlotWall - solid.c
                    - PlotObjWalls - solid.c
                  - StopAtWalls - solid.c
                    - TestWall - solid.c
                    - ReplotWalls - solid.c
                    - PlotObjWalls - solid.c
                      - PlotWall - solid.c
                      - PlotWallB - solid.c
                      - GoolCollide - gool.c
                  - StopAtCeil - solid.c
                    - FindCeilY - solid.c
                  - StopAtZone - solid.c
          - GoolObjectTransform - gool.c
            - GfxTransformSvtx - gfx.c
              - GfxCalcObjectMatrices - gfx.c
              - GpuGetPrimsTail - psx/gpu.c
              - RGteTransformSvtx - psx/r3000a.s
            - GfxTransformCvtx - gfx.c
              - RGteCalcSpriteRotMatrix - psx/r3000a.s
              - GfxCalcObjectMatrices - gfx.c
              - GpuGetPrimsTail - psx/gpu.c
              - RGteTransformCvtx - psx/r3000a.s
            - RGteCalcSpriteRotMatrix - psx/r3000a.s
            - RGteTransformSprite - psx/r3000a.s
            - GoolTextObjectTransform - gool.c
            - GfxTransformFragment - gfx.c
      - TitleUpdate - title.c
      - GpuUpdate - psx/gpu.c
        - AudioUpdate - audio.c
        - CardUpdate - psx/card.c
    - GpuUpdate - psx/gpu.c
    - NSKill - ns.c
  - kill - main.c
```
