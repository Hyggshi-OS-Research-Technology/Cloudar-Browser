let settingsBridge = null;
let saveTimeout = null;

const showToast = () => {
  const toast = document.getElementById('toast');
  toast.classList.add('show');
  clearTimeout(window.__toastTimer);
  window.__toastTimer = setTimeout(() => toast.classList.remove('show'), 1200);
};

const applySettingsToUI = (data) => {
  if (!data) return;
  const map = {
    homepage: 'homepage',
    search_engine: 'search_engine',
    startup: 'startup',
    downloads_folder: 'downloads_folder',
    theme: 'theme',
    tab_sleep_minutes: 'tab_sleep_minutes'
  };

  const checkboxMap = {
    incognito: 'incognito',
    ask_before_download: 'ask_before_download',
    hardware_accel: 'hardware_accel',
    vertical_tabs: 'vertical_tabs',
    vertical_tabs_collapsed: 'vertical_tabs_collapsed'
  };


  Object.entries(map).forEach(([key, id]) => {
    const el = document.getElementById(id);
    if (el && data[key] !== undefined) {
      el.value = data[key];
    }
  });

  Object.entries(checkboxMap).forEach(([key, id]) => {
    const el = document.getElementById(id);
    if (el && data[key] !== undefined) {
      el.checked = !!data[key];
    }
  });

  const collapsed = document.getElementById('vertical_tabs_collapsed');
  const verticalTabs = document.getElementById('vertical_tabs');
  if (collapsed && verticalTabs) {
    collapsed.disabled = !verticalTabs.checked;
  }

  const version = document.getElementById('version');

  if (version) version.textContent = data.version || '1.0.0';

  const engine = document.getElementById('engine');
  if (engine) engine.textContent = data.engine || 'Qt WebEngine';
};

const updateSetting = (key, value) => {
  if (!settingsBridge) return;
  settingsBridge.updateSettings({ [key]: value });
  showToast();
};

const debounceUpdate = (key, value) => {
  clearTimeout(saveTimeout);
  saveTimeout = setTimeout(() => updateSetting(key, value), 300);
};

const initNav = () => {
  const navItems = document.querySelectorAll('.nav-item');
  const sections = document.querySelectorAll('.section');

  const activateSection = (sectionId) => {
    const btn = document.querySelector(`.nav-item[data-section="${sectionId}"]`);
    const target = document.getElementById(sectionId);
    
    if (btn && target) {
      navItems.forEach((i) => i.classList.remove('active'));
      sections.forEach((s) => s.classList.remove('active'));
      
      btn.classList.add('active');
      target.classList.add('active');
      target.scrollIntoView({ block: 'start' });
    }
  };

  navItems.forEach((item) => {
    item.addEventListener('click', () => {
      activateSection(item.dataset.section);
      history.replaceState(null, '', '#' + item.dataset.section);
    });
  });

  const getInitialFragment = () => {
    const hash = window.location.hash.replace('#', '').toLowerCase().trim();
    if (hash) return hash;
    if (typeof window.INJECTED_FRAGMENT === 'string' && window.INJECTED_FRAGMENT) {
      return window.INJECTED_FRAGMENT.replace('#', '').toLowerCase().trim();
    }
    return '';
  };

  const attemptNav = () => {
    const fragment = getInitialFragment();
    if (fragment) activateSection(fragment);
  };

  window.addEventListener('hashchange', attemptNav);
  
  // Wait for QtWebEngine logic to settle before navigating initial state
  setTimeout(attemptNav, 150);
  attemptNav();
};

const initControls = () => {
  document.getElementById('homepage').addEventListener('input', (e) => {
    debounceUpdate('homepage', e.target.value);
  });

  document.getElementById('search_engine').addEventListener('input', (e) => {
    debounceUpdate('search_engine', e.target.value);
  });

  document.getElementById('startup').addEventListener('change', (e) => {
    updateSetting('startup', e.target.value);
  });

  document.getElementById('theme').addEventListener('change', (e) => {
    updateSetting('theme', e.target.value);
  });

  document.getElementById('incognito').addEventListener('change', (e) => {
    updateSetting('incognito', e.target.checked);
  });

  document.getElementById('ask_before_download').addEventListener('change', (e) => {
    updateSetting('ask_before_download', e.target.checked);
  });

  document.getElementById('hardware_accel').addEventListener('change', (e) => {
    updateSetting('hardware_accel', e.target.checked);
  });

  const verticalTabs = document.getElementById('vertical_tabs');
  const verticalTabsCollapsed = document.getElementById('vertical_tabs_collapsed');
  if (verticalTabs) {
    verticalTabs.addEventListener('change', (e) => {
      updateSetting('vertical_tabs', e.target.checked);
      if (verticalTabsCollapsed) verticalTabsCollapsed.disabled = !e.target.checked;
    });
  }
  if (verticalTabsCollapsed) {
    verticalTabsCollapsed.addEventListener('change', (e) => {
      updateSetting('vertical_tabs_collapsed', e.target.checked);
    });
  }

  const sleepInput = document.getElementById('tab_sleep_minutes');

  if (sleepInput) {
    sleepInput.addEventListener('input', (e) => {
      debounceUpdate('tab_sleep_minutes', e.target.value);
    });
  }

  document.getElementById('choose_folder').addEventListener('click', () => {
    if (!settingsBridge) return;
    settingsBridge.selectDownloadFolder((folder) => {
      if (folder) {
        document.getElementById('downloads_folder').value = folder;
      }
    });
  });

  document.getElementById('clear_data').addEventListener('click', () => {
    if (!settingsBridge) return;
    settingsBridge.clearBrowsingData();
  });

  const langSelect = document.getElementById('language-select');
  if (langSelect) {
    langSelect.addEventListener('change', (e) => {
      if (settingsBridge) settingsBridge.setLanguage(e.target.value);
    });
  }
};

const initChannel = () => {
  if (typeof qt === 'undefined') return;

  new QWebChannel(qt.webChannelTransport, (channel) => {
    settingsBridge = channel.objects.settingsBridge;
    if (!settingsBridge) return;

    settingsBridge.getSettings((data) => {
      applySettingsToUI(data);
    });

    if (settingsBridge.settingsChanged) {
      settingsBridge.settingsChanged.connect((data) => {
        applySettingsToUI(data);
      });
    }

    if (settingsBridge.getLanguageInfo) {
      settingsBridge.getLanguageInfo((info) => {
        const select = document.getElementById('language-select');
        if (!select || !info) return;
        select.innerHTML = '';
        const available = info.available || {};
        const current = info.current || 'en';
        for (const [code, name] of Object.entries(available)) {
            const opt = document.createElement('option');
            opt.value = code;
            opt.textContent = name;
            if (code === current) opt.selected = true;
            select.appendChild(opt);
        }
      });
    }
  });
};

window.addEventListener('DOMContentLoaded', () => {
  initNav();
  initControls();
  initChannel();
});
