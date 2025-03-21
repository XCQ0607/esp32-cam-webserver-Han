/*
 * primary HTML for the OV2640 camera module
 */

const uint8_t index_ov2640_html[] = R"=====(<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <title>ESP32 OV2640 摄像头</title>
    <link rel="icon" type="image/png" sizes="32x32" href="/favicon-32x32.png">
    <link rel="icon" type="image/png" sizes="16x16" href="/favicon-16x16.png">
    <link rel="stylesheet" type="text/css" href="/style.css">
    <style>
      @media (min-width: 800px) and (orientation:landscape) {
        #content {
          display:flex;
          flex-wrap: nowrap;
          align-items: stretch
        }
      }
    </style>
  </head>

  <body>
    <section class="main">
      <div id="logo">
        <label for="nav-toggle-cb" id="nav-toggle" style="float:left;">&#9776;&nbsp;&nbsp;设置&nbsp;&nbsp;&nbsp;&nbsp;</label>
        <button id="swap-viewer" style="float:left;" title="切换到简易查看器">简易</button>
        <button id="get-still" style="float:left;">拍照</button>
        <button id="toggle-stream" style="float:left;" class="hidden">开始视频流</button>
        <div id="wait-settings" style="float:left;" class="loader" title="等待摄像头设置加载"></div>
      </div>
      <div id="content">
        <div class="hidden" id="sidebar">
          <input type="checkbox" id="nav-toggle-cb" checked="checked">
            <nav id="menu">
              <div class="input-group hidden" id="lamp-group" title="闪光灯LED.&#013;&#013;警告:&#013;内置灯可能非常明亮！避免直视LED&#013;在高设置下可能会消耗大量电力，并可能导致视觉伪影，影响WiFi甚至导致摄像头电压不足">
                <label for="lamp">灯光</label>
                <div class="range-min">关</div>
                <input type="range" id="lamp" min="0" max="100" value="0" class="default-action">
                <div class="range-max"><span style="font-size: 125%;">&#9888;</span>最大</div>
              </div>
              <div class="input-group hidden" id="autolamp-group" title="启用后，灯光将只在摄像头激活时打开">
                <label for="autolamp">自动灯光</label>
                <div class="switch">
                  <input id="autolamp" type="checkbox" class="default-action">
                  <label class="slider" for="autolamp"></label>
                </div>
              </div>

              <div class="input-group" id="framesize-group" title="摄像头分辨率&#013;较高的分辨率将导致较低的帧率">
                <label for="framesize">分辨率</label>
                <select id="framesize" class="default-action">
                  <option value="13">UXGA (1600x1200)</option>
                  <option value="12">SXGA (1280x1024)</option>
                  <option value="11">HD (1280x720)</option>
                  <option value="10">XGA (1024x768)</option>
                  <option value="9">SVGA (800x600)</option>
                  <option value="8">VGA (640x480)</option>
                  <option value="7">HVGA (480x320)</option>
                  <option value="6">CIF (400x296)</option>
                  <option value="5">QVGA (320x240)</option>
                  <option value="3">HQVGA (240x176)</option>
                  <option value="1">QQVGA (160x120)</option>
                  <option value="0">THUMB (96x96)</option>
                </select>
              </div>
              <div class="input-group" id="quality-group" title="摄像头图像和流质量因素&#013;较高的设置将导致较低的帧率">
                <label for="quality">质量</label>
                <div class="range-min">低</div>
                <!-- 注意；以下元素在CSS中被"翻转"，因此它从高到低滑动
                     因此此处的"min"和"max"值也被反转 -->
                <input type="range" id="quality" min="6" max="63" value="10" class="default-action">
                <div class="range-max">高</div>
              </div>
              <div class="input-group" id="set-xclk-group" title="摄像头总线时钟频率&#013;增加此值将提高摄像头帧率和捕获速度&#013;&#013;设置过高将导致视觉伪影和/或不完整的帧&#013;此设置在不同板子间差异很大，预算型板子通常需要较低的值">
                 <label for="set-xclk">XCLK</label>
                 <div class="text">
                    <input id="xclk" type="number" min="2" max="32" size="3" step="1" class="default-action">
                    <div class="range-max">MHz</div>
                  </div>
              </div>
              <div class="input-group" id="brightness-group">
                <label for="brightness">亮度</label>
                <div class="range-min">-2</div>
                <input type="range" id="brightness" min="-2" max="2" value="0" class="default-action">
                <div class="range-max">2</div>
              </div>
              <div class="input-group" id="contrast-group">
                <label for="contrast">对比度</label>
                <div class="range-min">-2</div>
                <input type="range" id="contrast" min="-2" max="2" value="0" class="default-action">
                <div class="range-max">2</div>
              </div>
              <div class="input-group" id="saturation-group">
                <label for="saturation">饱和度</label>
                <div class="range-min">-2</div>
                <input type="range" id="saturation" min="-2" max="2" value="0" class="default-action">
                <div class="range-max">2</div>
              </div>
              <div class="input-group" id="special_effect-group">
                <label for="special_effect">特效</label>
                <select id="special_effect" class="default-action">
                  <option value="0" selected="selected">无特效</option>
                  <option value="1">负片</option>
                  <option value="2">灰度</option>
                  <option value="3">红色调</option>
                  <option value="4">绿色调</option>
                  <option value="5">蓝色调</option>
                  <option value="6">复古</option>
                </select>
              </div>
              <div class="input-group" id="awb-group">
                <label for="awb">自动白平衡</label>
                <div class="switch">
                  <input id="awb" type="checkbox" class="default-action" checked="checked">
                  <label class="slider" for="awb"></label>
                </div>
              </div>
              <div class="input-group" id="awb_gain-group">
                <label for="awb_gain">手动白平衡增益</label>
                <div class="switch">
                  <input id="awb_gain" type="checkbox" class="default-action" checked="checked">
                  <label class="slider" for="awb_gain"></label>
                </div>
              </div>
              <div class="input-group" id="wb_mode-group">
                <label for="wb_mode">白平衡模式</label>
                <select id="wb_mode" class="default-action">
                  <option value="0" selected="selected">自动</option>
                  <option value="1">晴天</option>
                  <option value="2">阴天</option>
                  <option value="3">办公室</option>
                  <option value="4">家庭</option>
                </select>
              </div>
              <div class="input-group" id="aec-group">
                <label for="aec">自动曝光控制传感器</label>
                <div class="switch">
                  <input id="aec" type="checkbox" class="default-action" checked="checked">
                  <label class="slider" for="aec"></label>
                </div>
              </div>
              <div class="input-group" id="aec2-group">
                <label for="aec2">自动曝光控制DSP</label>
                <div class="switch">
                  <input id="aec2" type="checkbox" class="default-action" checked="checked">
                  <label class="slider" for="aec2"></label>
                </div>
              </div>
              <div class="input-group" id="ae_level-group">
                <label for="ae_level">自动曝光等级</label>
                <div class="range-min">-2</div>
                <input type="range" id="ae_level" min="-2" max="2" value="0" class="default-action">
                <div class="range-max">2</div>
              </div>
              <div class="input-group" id="aec_value-group">
                <label for="aec_value">曝光</label>
                <div class="range-min">0</div>
                <input type="range" id="aec_value" min="0" max="1200" value="204" class="default-action">
                <div class="range-max">1200</div>
              </div>
              <div class="input-group" id="agc-group">
                <label for="agc">自动增益控制</label>
                <div class="switch">
                  <input id="agc" type="checkbox" class="default-action" checked="checked">
                  <label class="slider" for="agc"></label>
                </div>
              </div>
              <div class="input-group hidden" id="agc_gain-group">
                <label for="agc_gain">增益</label>
                <div class="range-min">1x</div>
                <input type="range" id="agc_gain" min="0" max="30" value="5" class="default-action">
                <div class="range-max">31x</div>
              </div>
              <div class="input-group" id="gainceiling-group">
                <label for="gainceiling">增益上限</label>
                <div class="range-min">2x</div>
                <input type="range" id="gainceiling" min="0" max="6" value="0" class="default-action">
                <div class="range-max">128x</div>
              </div>
              <div class="input-group" id="bpc-group">
                <label for="bpc">黑点校正</label>
                <div class="switch">
                  <input id="bpc" type="checkbox" class="default-action">
                  <label class="slider" for="bpc"></label>
                </div>
              </div>
              <div class="input-group" id="wpc-group">
                <label for="wpc">白点校正</label>
                <div class="switch">
                  <input id="wpc" type="checkbox" class="default-action" checked="checked">
                  <label class="slider" for="wpc"></label>
                </div>
              </div>
              <div class="input-group" id="raw_gma-group">
                <label for="raw_gma">原始伽马启用</label>
                <div class="switch">
                  <input id="raw_gma" type="checkbox" class="default-action" checked="checked">
                  <label class="slider" for="raw_gma"></label>
                </div>
              </div>
              <div class="input-group" id="lenc-group">
                <label for="lenc">镜头校正</label>
                <div class="switch">
                  <input id="lenc" type="checkbox" class="default-action" checked="checked">
                  <label class="slider" for="lenc"></label>
                </div>
              </div>
              <div class="input-group" id="hmirror-group">
                <label for="hmirror">水平镜像</label>
                <div class="switch">
                  <input id="hmirror" type="checkbox" class="default-action" checked="checked">
                  <label class="slider" for="hmirror"></label>
                </div>
              </div>
              <div class="input-group" id="vflip-group">
                <label for="vflip">垂直翻转</label>
                <div class="switch">
                  <input id="vflip" type="checkbox" class="default-action" checked="checked">
                  <label class="slider" for="vflip"></label>
                </div>
              </div>
              <div class="input-group" id="rotate-group">
                <label for="rotate">浏览器中旋转</label>
                <select id="rotate" class="default-action">
                  <option value="90">90&deg; (向右)</option>
                  <option value="0" selected="selected">0&deg; (无)</option>
                  <option value="-90">-90&deg; (向左)</option>
                </select>
              </div>
              <div class="input-group" id="dcw-group">
                <label for="dcw">下采样使能</label>
                <div class="switch">
                  <input id="dcw" type="checkbox" class="default-action" checked="checked">
                  <label class="slider" for="dcw"></label>
                </div>
              </div>
              <div class="input-group" id="colorbar-group">
                <label for="colorbar">测试图案</label>
                <div class="switch">
                  <input id="colorbar" type="checkbox" class="default-action">
                  <label class="slider" for="colorbar"></label>
                </div>
              </div>
              <div class="input-group" id="min_frame_time-group" title="最小帧时间&#013;较高的设置会降低帧率&#013;使用此功能可以使视频流更平滑并减少WiFi和浏览器的负载">
                <label for="min_frame_time">帧持续时间限制</label>
                <select id="min_frame_time" class="default-action">
                  <option value="3333">3.3秒  (0.3帧/秒)</option>
                  <option value="2000">2秒    (0.5帧/秒)</option>
                  <option value="1000">1秒    (1帧/秒)</option>
                  <option value="500">500毫秒 (2帧/秒)</option>
                  <option value="333">333毫秒 (3帧/秒)</option>
                  <option value="250">250毫秒 (4帧/秒)</option>
                  <option value="200">200毫秒 (5帧/秒)</option>
                  <option value="166">166毫秒 (6帧/秒)</option>
                  <option value="142">142毫秒 (7帧/秒)</option>
                  <option value="111">111毫秒 (9帧/秒)</option>
                  <option value="83">83毫秒 (12帧/秒)</option>
                  <option value="71">71毫秒 (14帧/秒)</option>
                  <option value="55">55毫秒 (18帧/秒)</option>
                  <option value="41">41毫秒 (24帧/秒)</option>
                </select>
              </div>
              <div class="input-group" id="preferences-group">
                <label for="prefs" style="line-height: 2em;">偏好设置</label>
                <button id="reboot" title="重启摄像头模块">重启</button>
                <button id="save_prefs" title="在摄像头模块上保存偏好设置">保存</button>
                <button id="clear_prefs" title="在摄像头模块上擦除保存的偏好设置">清除</button>
              </div>
              <div class="input-group" id="cam_name-group">
                <label for="cam_name">
                <a href="/dump" title="系统信息" target="_blank">名称</a></label>
                <div id="cam_name" class="default-action"></div>
              </div>
              <div class="input-group" id="code_ver-group">
                <label for="code_ver">
                <a href="https://github.com/easytarget/esp32-cam-webserver"
                  title="GitHub上的ESP32摄像头网络服务器" target="_blank">固件</a></label>
                <div id="code_ver" class="default-action"></div>
              </div>
              <div class="input-group hidden" id="stream-group">
                <label for="stream_url" id="stream_link">视频流</label>
                <div id="stream_url" class="default-action">未知</div>
              </div>
            </nav>
        </div>
        <figure>
          <div id="stream-container" class="image-container hidden">
            <div class="close close-rot-none" id="close-stream">×</div>
            <img id="stream" src="">
          </div>
        </figure>
      </div>
    </section>
  </body>

  <script>
  document.addEventListener('DOMContentLoaded', function (event) {
    var baseHost = document.location.origin;
    var streamURL = '未定义';
    var viewerURL = '未定义';

    const header = document.getElementById('logo')
    const settings = document.getElementById('sidebar')
    const waitSettings = document.getElementById('wait-settings')
    const lampGroup = document.getElementById('lamp-group')
    const autolampGroup = document.getElementById('autolamp-group')
    const streamGroup = document.getElementById('stream-group')
    const camName = document.getElementById('cam_name')
    const codeVer = document.getElementById('code_ver')
    const rotate = document.getElementById('rotate')
    const view = document.getElementById('stream')
    const viewContainer = document.getElementById('stream-container')
    const stillButton = document.getElementById('get-still')
    const streamButton = document.getElementById('toggle-stream')
    const closeButton = document.getElementById('close-stream')
    const streamLink = document.getElementById('stream_link')
    const framesize = document.getElementById('framesize')
    const xclk = document.getElementById('xclk')
    const swapButton = document.getElementById('swap-viewer')
    const savePrefsButton = document.getElementById('save_prefs')
    const clearPrefsButton = document.getElementById('clear_prefs')
    const rebootButton = document.getElementById('reboot')
    const minFrameTime = document.getElementById('min_frame_time')

    const hide = el => {
      el.classList.add('hidden')
    }
    const show = el => {
      el.classList.remove('hidden')
    }

    const disable = el => {
      el.classList.add('disabled')
      el.disabled = true
    }

    const enable = el => {
      el.classList.remove('disabled')
      el.disabled = false
    }

    const updateValue = (el, value, updateRemote) => {
      updateRemote = updateRemote == null ? true : updateRemote
      let initialValue
      if (el.type === 'checkbox') {
        initialValue = el.checked
        value = !!value
        el.checked = value
      } else {
        initialValue = el.value
        el.value = value
      }

      if (updateRemote && initialValue !== value) {
        updateConfig(el);
      } else if(!updateRemote){
        if(el.id === "aec"){
          value ? hide(exposure) : show(exposure)
        } else if(el.id === "agc"){
          if (value) {
            show(gainCeiling)
            hide(agcGain)
          } else {
            hide(gainCeiling)
            show(agcGain)
          }
        } else if(el.id === "awb_gain"){
          value ? show(wb) : hide(wb)
        } else if(el.id === "lamp"){
          if (value == -1) {
            hide(lampGroup)
            hide(autolampGroup)
          } else {
            show(lampGroup)
            show(autolampGroup)
          }
        } else if(el.id === "cam_name"){
          camName.innerHTML = value;
          window.document.title = value;
          console.log('Name set to: ' + value);
        } else if(el.id === "code_ver"){
          codeVer.innerHTML = value;
          console.log('Firmware Build: ' + value);
        } else if(el.id === "rotate"){
          rotate.value = value;
          applyRotation();
        } else if(el.id === "min_frame_time"){
          min_frame_time.value = value;
        } else if(el.id === "stream_url"){
          streamURL = value;
          viewerURL = value + 'view';
          stream_url.innerHTML = value;
          stream_link.setAttribute("title", `打开独立的视频流查看器 :: ${viewerURL}`);
          stream_link.style.textDecoration = "underline";
          stream_link.style.cursor = "pointer";
          streamButton.setAttribute("title", `开始视频流 :: ${streamURL}`);
          show(streamGroup)
          console.log('视频流URL设置为: ' + streamURL);
          console.log('视频流查看器URL设置为: ' + viewerURL);
        }
      }
    }

    var rangeUpdateScheduled = false
    var latestRangeConfig

    function updateRangeConfig (el) {
      latestRangeConfig = el
      if (!rangeUpdateScheduled) {
        rangeUpdateScheduled = true;
        setTimeout(function(){
          rangeUpdateScheduled = false
          updateConfig(latestRangeConfig)
        }, 150);
      }
    }

    function updateConfig (el) {
      let value
      switch (el.type) {
        case 'checkbox':
          value = el.checked ? 1 : 0
          break
        case 'range':
        case 'number':
        case 'select-one':
          value = el.value
          break
        case 'button':
        case 'submit':
          value = '1'
          break
        default:
          return
      }

      const query = `${baseHost}/control?var=${el.id}&val=${value}`

      fetch(query)
        .then(response => {
          console.log(`request to ${query} finished, status: ${response.status}`)
        })
    }

    document
      .querySelectorAll('.close')
      .forEach(el => {
        el.onclick = () => {
          hide(el.parentNode)
        }
      })

    // 读取初始值
    fetch(`${baseHost}/status`)
      .then(function (response) {
        return response.json()
      })
      .then(function (state) {
        document
          .querySelectorAll('.default-action')
          .forEach(el => {
            updateValue(el, state[el.id], false)
          })
        hide(waitSettings);
        show(settings);
        show(streamButton);
        //startStream();
      })

    // 在"拍照"按钮上添加一些帮助文本
    stillButton.setAttribute("title", `捕获静态图像 :: ${baseHost}/capture`);

    const stopStream = () => {
      window.stop();
      streamButton.innerHTML = '开始视频流';
      streamButton.setAttribute("title", `开始视频流 :: ${streamURL}`);
      hide(viewContainer);
    }

    const startStream = () => {
      view.src = streamURL;
      view.scrollIntoView(false);
      streamButton.innerHTML = '停止视频流';
      streamButton.setAttribute("title", `停止视频流`);
      show(viewContainer);
    }

    const applyRotation = () => {
      rot = rotate.value;
      if (rot == -90) {
        viewContainer.style.transform = `rotate(-90deg)  translate(-100%)`;
        closeButton.classList.remove('close-rot-none');
        closeButton.classList.remove('close-rot-right');
        closeButton.classList.add('close-rot-left');
      } else if (rot == 90) {
        viewContainer.style.transform = `rotate(90deg) translate(0, -100%)`;
        closeButton.classList.remove('close-rot-left');
        closeButton.classList.remove('close-rot-none');
        closeButton.classList.add('close-rot-right');
      } else {
        viewContainer.style.transform = `rotate(0deg)`;
        closeButton.classList.remove('close-rot-left');
        closeButton.classList.remove('close-rot-right');
        closeButton.classList.add('close-rot-none');
      }
      console.log('旋转 ' + rot + ' 已应用');
    }

    // 为控件绑定动作

    streamLink.onclick = () => {
      stopStream();
      window.open(viewerURL, "_blank");
    }

    stillButton.onclick = () => {
      stopStream();
      view.src = `${baseHost}/capture?_cb=${Date.now()}`;
      view.scrollIntoView(false);
      show(viewContainer);
    }

    closeButton.onclick = () => {
      stopStream();
      hide(viewContainer);
    }

    streamButton.onclick = () => {
      const streamEnabled = streamButton.innerHTML === '停止视频流'
      if (streamEnabled) {
        stopStream();
      } else {
        startStream();
      }
    }

    // Attach default on change action
    document
      .querySelectorAll('.default-action')
      .forEach(el => {
        el.onchange = () => updateConfig(el)
      })

    // Update range sliders as they are being moved
    document
      .querySelectorAll('input[type="range"]')
      .forEach(el => {
        el.oninput = () => updateRangeConfig(el)
      })

    // Custom actions
    // Gain
    const agc = document.getElementById('agc')
    const agcGain = document.getElementById('agc_gain-group')
    const gainCeiling = document.getElementById('gainceiling-group')
    agc.onchange = () => {
      updateConfig(agc)
      if (agc.checked) {
        show(gainCeiling)
        hide(agcGain)
      } else {
        hide(gainCeiling)
        show(agcGain)
      }
    }

    // Exposure
    const aec = document.getElementById('aec')
    const exposure = document.getElementById('aec_value-group')
    aec.onchange = () => {
      updateConfig(aec)
      aec.checked ? hide(exposure) : show(exposure)
    }

    // AWB
    const awb = document.getElementById('awb_gain')
    const wb = document.getElementById('wb_mode-group')
    awb.onchange = () => {
      updateConfig(awb)
      awb.checked ? show(wb) : hide(wb)
    }

    // Detection and framesize
    rotate.onchange = () => {
      applyRotation();
      updateConfig(rotate);
    }

    framesize.onchange = () => {
      updateConfig(framesize)
    }

    minFrameTime.onchange = () => {
      updateConfig(minFrameTime)
    }

    xclk.onchange = () => {
      console.log("xclk:" , xclk);
      updateConfig(xclk)
    }

    swapButton.onclick = () => {
      window.open('/?view=simple','_self');
    }

    savePrefsButton.onclick = () => {
      if (confirm("保存当前设置?")) {
        updateConfig(savePrefsButton);
      }
    }

    clearPrefsButton.onclick = () => {
      if (confirm("Remove the saved preferences?")) {
        updateConfig(clearPrefsButton);
      }
    }

    rebootButton.onclick = () => {
      if (confirm("Reboot the Camera Module?")) {
        updateConfig(rebootButton);
        // Some sort of countdown here?
        hide(settings);
        hide(viewContainer);
        header.innerHTML = '<h1>Rebooting!</h1><hr>Page will reload after 30 seconds.';
        setTimeout(function() {
          location.replace(document.URL);
        }, 30000);
      }
    }

  })
  </script>
</html>)=====";

size_t index_ov2640_html_len = sizeof(index_ov2640_html)-1;
