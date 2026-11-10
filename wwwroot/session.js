'use strict';

window.JiaoSession = (() => {
  let leaving = false;
  let loggingOut = false;
  let logoutBound = false;

  const element = id => document.getElementById(id);
  const defaultAvatar = '/avaters/commen_path.png';
  const roleName = role => ['teacher', '教师', '老师'].includes(role) ? '教师'
    : (['student', '学生'].includes(role) ? '学生' : '用户');

  function avatarUrl(user) {
    const value = user && (user.avatar_url || user.avatar || user.avater);
    if (typeof value !== 'string') return defaultAvatar;
    // 兼容后端返回的 ./wwwroot/avaters/... 磁盘路径。
    const url = value.replace(/^(?:\.\/|\/)?wwwroot\//, '/');
    return /^\/avaters\/(?:\d+|commen_path)\.png$/.test(url) ? url : defaultAvatar;
  }

  function renderAvatar(target, user) {
    if (!target) return;
    const img = document.createElement('img');
    img.alt = '';
    const url = avatarUrl(user);
    img.src = url === defaultAvatar ? url : url + '?v=' + Date.now();
    img.onerror = () => {
      img.onerror = null;
      img.src = defaultAvatar;
    };
    target.replaceChildren(img);
  }

  async function request(url, options = {}) {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 12000);
    try {
      const response = await fetch(url, {
        ...options, credentials: 'same-origin', cache: 'no-store', signal: controller.signal
      });
      const data = await response.json();
      if (!response.ok || !data || data.success !== true) {
        throw new Error(data && typeof data.message === 'string'
          ? data.message : '服务响应异常，请稍后重试。');
      }
      return data;
    } finally {
      clearTimeout(timeout);
    }
  }

  async function fetchUser(required, endpoint) {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 12000);
    try {
      const response = await fetch(endpoint, {
        headers: { 'Accept': 'application/json' },
        credentials: 'same-origin', cache: 'no-store', signal: controller.signal
      });
      if (response.status === 401) {
        if (required) redirectIfUnauthorized(response);
        return null;
      }
      const data = await response.json();
      if (!response.ok || !data || data.success !== true || !data.user ||
          typeof data.user.idcount !== 'string' || typeof data.user.identity !== 'string' ||
          (data.user.name !== undefined && typeof data.user.name !== 'string')) {
        throw new Error('暂时无法获取个人信息，请稍后重试。');
      }
      return data.user;
    } catch (error) {
      if (error instanceof SyntaxError) throw new Error('个人信息响应异常，请稍后重试。');
      throw error;
    } finally {
      clearTimeout(timeout);
    }
  }

  function renderUser(user) {
    const name = user.name || user.idcount;
    element('session-name').textContent = name;
    element('session-name').title = user.idcount;
    renderAvatar(element('session-avatar'), user);
    element('session-role').textContent = roleName(user.identity);
    element('session-info').hidden = false;
    const active = element('active-session');
    if (active) active.hidden = false;
  }

  function redirectIfUnauthorized(response) {
    if (response.status !== 401) return false;
    if (!leaving) {
      leaving = true;
      window.location.replace('/index.html?reason=expired#login');
    }
    return true;
  }

  function showError(error) {
    const notice = element('session-notice');
    if (!notice || leaving) return;
    notice.textContent = error.name === 'AbortError'
      ? '请求超时，请检查网络后重试。' : error.message;
    notice.hidden = false;
  }

  async function logout() {
    if (loggingOut || leaving) return;
    const button = element('logout-button');
    loggingOut = true;
    button.disabled = true;
    button.textContent = '正在退出…';
    try {
      await request('/api/logout', {
        method: 'POST', headers: { 'Accept': 'application/json' }
      });
      leaving = true;
      window.location.replace('/index.html?reason=logout#login');
    } catch (error) {
      showError(error);
    } finally {
      loggingOut = false;
      button.disabled = false;
      button.textContent = '退出登录';
    }
  }

  async function init({ required = false, endpoint = '/api/me' } = {}) {
    if (!logoutBound) {
      const button = element('logout-button');
      if (button) {
        button.addEventListener('click', logout);
        logoutBound = true;
      }
    }
    try {
      const user = await fetchUser(required, endpoint);
      if (!user || leaving) return null;
      renderUser(user);
      return user;
    } catch (error) {
      if (required) showError(error);
      return null;
    }
  }

  window.addEventListener('pageshow', event => {
    if (event.persisted) window.location.reload();
  });

  return { init, redirectIfUnauthorized, roleName, avatarUrl, renderAvatar };
})();
