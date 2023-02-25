// Note: The `Module` context is already initialized as an
// empty object by emscripten even before the pre script
Object.assign(Module = {
  EASYRPG_GAME: "",

  preRun: [onPreRun],
  postRun: [],

  print: (...args) => {
    console.log(...args);
  },

  printErr: (...args) => {
    console.error(...args);
  },

  canvas: (() => {
    const canvas = document.getElementById('canvas');

    // As a default initial behavior, pop up an alert when webgl context is lost
    // See http://www.khronos.org/registry/webgl/specs/latest/1.0/#5.15.2
    canvas.addEventListener('webglcontextlost', event => {
      alert('WebGL context lost. You will need to reload the page.');
      event.preventDefault();
    }, false);

    return canvas;
  })(),

  setStatus: text => {
    if (!Module.setStatus.last) Module.setStatus.last = {
      time: Date.now(),
      text: ''
    };

    if (text !== Module.setStatus.text) {
      const statusLabel = document.getElementById('status');
      if (statusLabel)
        statusLabel.innerHTML = text;
    }
  },

  totalDependencies: 0,

  monitorRunDependencies: left => {
    Module.totalDependencies = Math.max(Module.totalDependencies, left);
    Module.setStatus(left ? `Preparing... (${Module.totalDependencies - left}/${Module.totalDependencies})` : 'Downloading game data...');
  }
});

var game_pushed = false;
var lang_pushed = false;
/**
 * Parses the current location query to setup a specific game
 */
function parseArgs () {
  const items = window.location.search.substr(1).split("&");
  let result = [];

  // Store saves in subdirectory `Save`
  result.push("--save-path");
  result.push("Save");

  for (let i = 0; i < items.length; i++) {
    const tmp = items[i].split("=");

    if (tmp[0] === "project-path" || tmp[0] === "save-path") {
      // Filter arguments that are set by us
      continue;
    }

    // Filesystem is not ready when processing arguments, store path to game/language
    if (tmp[0] === "game" && tmp.length > 1) {
      Module.EASYRPG_GAME = tmp[1].toLowerCase();
      game_pushed = true;
    } else if (tmp[0] === "language" && tmp.length > 1) {
      Module.EASYRPG_LANGUAGE = decodeURI(tmp[1]);
      lang_pushed = true;
    }

    if (tmp.length > 1) {
      const arg = decodeURI(tmp[1]);
      // Split except if it's a string
      if (arg.length > 0) {
        if (arg.startsWith('"') && arg.endsWith('"')) {
          result.push(arg.slice(1, -1));
        } else {
          result = [...result, ...arg.split(" ")];
        }
      }
    }
  }

  return result;
}

function onPreRun () {
  // Retrieve save directory from persistent storage before using it
  FS.mkdir("Save");
  FS.mount(Module.EASYRPG_FS, {}, 'Save');

  // For preserving the configuration. Shared across website
  FS.mkdir("/home/web_user/.config");
  FS.mount(IDBFS, {}, '/home/web_user/.config');

  FS.syncfs(true, function(err) {});
}

Module.setStatus('Downloading...');
Module.arguments = ["easyrpg-player", ...parseArgs()];

if (Module.EASYRPG_GAME === undefined) {
  Module.EASYRPG_GAME = "";
} else if (!game_pushed) {
  Module.arguments.push("--game", Module.EASYRPG_GAME);
}

if (Module.EASYRPG_LANGUAGE === undefined || Module.EASYRPG_LANGUAGE.toLowerCase() === "default") {
  Module.EASYRPG_LANGUAGE = "";
} else if (!lang_pushed) {
  Module.arguments.push("--language", Module.EASYRPG_LANGUAGE);
}

if (Module.EASYRPG_WS_URL === undefined) {
  Module.EASYRPG_WS_URL = "ws://localhost:8080/";
}

// Catch all errors occuring inside the window
window.addEventListener('error', (event) => {
  // workaround chrome bug: See https://github.com/EasyRPG/Player/issues/2806
  if (event.error.message.includes("side-effect in debug-evaluate") && event.defaultPrevented) {
    return;
  }

  Module.setStatus('Exception thrown, see JavaScript console…');
  Module.setStatus = text => {
    if (text) Module.printErr(`[post-exception status] ${text}`);
  };
});
