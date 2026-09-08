-- GPL-3.0. Actual Scene/SceneAnim/AAA smoke test, with a Freestyle mesh fixture.
local hg = require("harfang")
io.stdout:setvbuf("no")
local compiled = assert(os.getenv("FREESTYLE_HG_COMPILED"))
local output = assert(os.getenv("FREESTYLE_HG_OUTPUT"))
local function record(s) print("PROBE " .. s) end
hg.InputInit()
hg.WindowSystemInit()
local win = hg.NewWindow("Freestyle HARFANG feasibility", 640, 480, 32, hg.WV_Hidden)
assert(hg.RenderInit(win, hg.RT_Direct3D11))
hg.AddAssetsFolder(compiled)
local pipeline, resources, scene = hg.CreateForwardPipeline(), hg.PipelineResources(), hg.Scene()
assert(hg.LoadSceneFromAssets("fixture.scn", scene, resources, hg.GetForwardPipelineInfo()))
local camera = hg.CreateCamera(scene, hg.TranslationMat4(hg.Vec3(0.3, 1.3, -4)), 0.01, 100, hg.Deg(40))
scene:SetCurrentCamera(camera)
hg.CreatePointLight(scene, hg.TranslationMat4(hg.Vec3(-2, 3, -3)), 15, hg.Color(1, 0.9, 0.8), 10)
hg.CreatePointLight(scene, hg.TranslationMat4(hg.Vec3(2, 1, 2)), 15, hg.Color(0.5, 0.6, 1), 4)
local config = hg.ForwardPipelineAAAConfig()
config.sample_count = 1
config.motion_blur = 0
local aaa = hg.CreateForwardPipelineAAAFromAssets("core", config)
assert(hg.IsValid(aaa), "AAA resources failed")
record('{"language":"Lua","nodes":' .. scene:GetNodeCount() .. ',"aaa_valid":true}')
for _,name in ipairs({"BindAnim", "EvaluateBoundAnim", "AddAnim", "GetAnims"}) do
    local ok, value = pcall(function() return scene[name] end)
    record('{"api":"' .. name .. '","available":' .. tostring(ok and value ~= nil) .. '}')
end
local parent, head = scene:GetNode("AnimatedParent"), scene:GetNode("FreestyleHead")
for _,name in ipairs({"linear_probe", "step_probe"}) do
    for _,t in ipairs({0, 0.25, 0.5, 1, 1.5, 2.5}) do
        scene:StopAllAnims()
        scene:PlayAnim(scene:GetSceneAnim(name), hg.ALM_Once, hg.E_Linear,
            hg.time_from_sec_f(t), hg.time_from_sec(3), true)
        scene:Update(0)
        record(string.format('{"animation":"%s","time":%.6f,"parent_x":%.9f,"child_world_x":%.9f}',
            name, t, parent:GetTransform():GetPos().x, hg.GetT(head:GetTransform():GetWorld()).x))
    end
end
scene:StopAllAnims()
scene:PlayAnim(scene:GetSceneAnim("linear_probe"), hg.ALM_Once, hg.E_Linear,
    hg.time_from_sec_f(0.5), hg.time_from_sec(3), true)
scene:Update(0)
local target_color = hg.CreateTexture(640, 480, "capture-color", hg.TF_RenderTarget, hg.TF_RGBA8)
local target_depth = hg.CreateTexture(640, 480, "capture-depth", hg.TF_RenderTarget, hg.TF_D24)
local framebuffer = hg.CreateFrameBuffer(target_color, target_depth, "aaa-capture")
local color = resources:AddTexture("aaa-capture-color", target_color)
local readback = hg.CreateTexture(640, 480, "readback", hg.TF_ReadBack | hg.TF_BlitDestination, hg.TF_RGBA8)
local picture = hg.Picture(640, 480, hg.PF_RGBA32)
local frame = 0
for i=1,64 do
    hg.SubmitSceneToPipeline(0, scene, hg.IntRect(0,0,640,480), true,
        pipeline, resources, aaa, config, frame, framebuffer.handle)
    frame = hg.Frame()
end
local ready = hg.CaptureTexture(200, resources, color, readback, picture)
while frame <= ready do frame=hg.Frame() end
assert(hg.SavePNG(picture, output .. "/lua-aaa.png"))
record('{"capture":"lua-aaa.png","frames":64}')
if os.getenv("FREESTYLE_HG_AUDIO") == "1" then
assert(hg.AudioInit())
local source = hg.StreamModuleFileStereo(compiled .. "/Mush.xm", hg.StereoSourceState(0))
record(string.format('{"xm_source":%d,"duration_seconds":%.6f}', source,
    hg.time_to_sec_f(hg.GetSourceDuration(source))))
hg.Sleep(hg.time_from_ms(250))
for _,t in ipairs({0, 5, 120}) do
    local accepted = hg.SetSourceTimecode(source, hg.time_from_sec(t))
    hg.Sleep(hg.time_from_ms(100))
    record(string.format('{"xm_seek":%d,"accepted":%s,"timecode_seconds":%.6f}', t,
        tostring(accepted), hg.time_to_sec_f(hg.GetSourceTimecode(source))))
end
hg.AudioShutdown()
end
hg.DestroyForwardPipelineAAA(aaa)
hg.DestroyFrameBuffer(framebuffer)
hg.DestroyTexture(target_depth)
hg.DestroyTexture(readback)
hg.DestroyForwardPipeline(pipeline)
scene:Clear()
resources:DestroyAllTextures()
resources:DestroyAllModels()
resources:DestroyAllPrograms()
hg.RenderShutdown()
hg.DestroyWindow(win)
hg.InputShutdown()
hg.WindowSystemShutdown()
record('{"completed":true}')
