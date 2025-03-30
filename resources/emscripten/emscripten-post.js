// Move to different directory to prevent save file collisions in IDBFS
FS.mkdir("easyrpg");
FS.chdir("easyrpg");

if (Module.game.length > 0) {
  FS.mkdir(Module.game);
  FS.chdir(Module.game);
}

// Use IDBFS for save file storage when the filesystem was not
// overwritten by a custom emscripten shell file
if (Module.saveFs === undefined) {
  Module.saveFs = IDBFS;
}

Module.initApi = function() {
  Module.api_private.download_js = function (buffer, size, filename) {
    const blob = new Blob([Module.HEAPU8.slice(buffer, buffer + size)]);
    const link = document.createElement('a');
    link.href = window.URL.createObjectURL(blob);
    link.download = UTF8ToString(filename);
    link.click();
    link.remove();
  }

  Module.api_private.createInputElement_js = function (id, accept, event) {
    let file = document.getElementById(id);
    if (file == null) {
      file = document.createElement('input');
      file.type = 'file';
      file.id = id;
      file.style.display = 'none';
      if (accept) file.accept = accept;
      file.addEventListener('change', function (evt) {
        const selected_file = evt.target.files[0];
        const reader = new FileReader();
        reader.onload = function(file) {
          event(file, selected_file.name);
        }
        reader.readAsArrayBuffer(selected_file);
      });
    }
    file.click();
  }

  Module.api_private.uploadSavegame_js = function (slot) {
    Module.api_private.createInputElement_js('easyrpg_saveFile', '.lsd', function (file) {
      const result = new Uint8Array(file.currentTarget.result);
      var buf = Module._malloc(result.length);
      Module.HEAPU8.set(result, buf);
      Module.api_private.uploadSavegameStep2(slot, buf, result.length);
      Module._free(buf);
      Module.api.refreshScene();
    });
  }

  Module.api_private.uploadSoundfont_js = function () {
    Module.api_private.createInputElement_js('easyrpg_sfFile', '.sf2', function (file, name) {
      const result = new Uint8Array(file.currentTarget.result);
      //const name_buf = Module._malloc(name.length + 1);
      //stringToUTF8(name, name_buf, name.length + 1);
      const content_buf = Module._malloc(result.length);
      Module.HEAPU8.set(result, content_buf);
      Module.api_private.uploadSoundfontStep2(name, content_buf, result.length);
      //Module._free(name_buf);
      Module._free(content_buf);
      Module.api.refreshScene();
    });
  }

  Module.api_private.uploadFont_js = function () {
    Module.api_private.createInputElement_js('easyrpg_fontFile', '.ttf,.otf,.bdf,.woff,.svg', function (file, name) {
      const result = new Uint8Array(file.currentTarget.result);
      //const name_buf = Module._malloc(name.length + 1);
      //stringToUTF8(name, name_buf, name.length + 1);
      const content_buf = Module._malloc(result.length);
      Module.HEAPU8.set(result, content_buf);
      Module.api_private.uploadFontStep2(name, content_buf, result.length);
      //Module._free(name_buf);
      Module._free(content_buf);
      Module.api.refreshScene();
    });
  }
}

// Display the nice end message forever
Module["onExit"] = function() {
  // load image
  let imageContent = FS.readFile("/tmp/message.png");
  var img = document.createElement('img');
  img.id = "canvas";
  img.src = URL.createObjectURL(new Blob([imageContent], {type: "image/png"}));

  // replace canvas
  var cvs = document.getElementById('canvas');
  cvs.parentNode.replaceChild(img, cvs);
}
