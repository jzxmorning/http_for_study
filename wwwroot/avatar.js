'use strict';

window.JiaoAvatar = (() => {
  const element = id => document.getElementById(id);
  const form = element('avatar-form');
  const input = element('avatar-file');
  const choose = element('avatar-choose');
  const save = element('avatar-save');
  const cancel = element('avatar-cancel');
  const actions = element('avatar-actions');
  const preview = element('user-avatar');
  const status = element('avatar-status');
  let currentUser = null;
  let selected = null;
  let previewUrl = '';
  let selectionVersion = 0;
  let uploading = false;

  function message(text, state) {
    status.textContent = text;
    status.dataset.state = state;
    status.hidden = false;
  }

  function clearSelection() {
    ++selectionVersion;
    selected = null;
    if (previewUrl) URL.revokeObjectURL(previewUrl);
    previewUrl = '';
    input.value = '';
    actions.hidden = true;
    save.disabled = true;
    preview.dataset.preview = 'false';
    JiaoSession.renderAvatar(preview, currentUser);
  }

  function setUser(user) {
    currentUser = user;
    if (!selected && !previewUrl) JiaoSession.renderAvatar(preview, user);
  }

  choose.addEventListener('click', () => {
    if (!uploading) input.click();
  });

  input.addEventListener('change', async () => {
    if (uploading) return;
    const file = input.files && input.files[0];
    if (!file) return;
    clearSelection();
    const version = selectionVersion;
    if (file.type !== 'image/png') {
      message('请选择 PNG 格式的图片。', 'error');
      return;
    }
    if (file.size === 0 || file.size > 10 * 1024 * 1024) {
      message(file.size === 0 ? '图片内容为空，请重新选择。' : '图片不能超过 10 MB。', 'error');
      return;
    }
    message('正在检查图片…', 'pending');
    previewUrl = URL.createObjectURL(file);
    const url = previewUrl;
    const image = new Image();
    try {
      await new Promise((resolve, reject) => {
        image.onload = resolve;
        image.onerror = () => reject(new Error('无法读取图片，请选择完整的 PNG 图片。'));
        image.src = url;
      });
      if (version !== selectionVersion) return;
      if (!image.naturalWidth || !image.naturalHeight || image.naturalWidth > 2048 || image.naturalHeight > 2048) {
        throw new Error('图片宽、高均不能超过 2048 像素，请缩小后重试。');
      }
      selected = file;
      image.alt = '';
      preview.replaceChildren(image);
      preview.dataset.preview = 'true';
      actions.hidden = false;
      save.disabled = false;
      message(`预览中 · ${file.name} · ${image.naturalWidth} × ${image.naturalHeight}，点击“保存头像”后生效。`, 'pending');
    } catch (error) {
      if (version !== selectionVersion) return;
      clearSelection();
      message(error.message, 'error');
    }
  });

  cancel.addEventListener('click', () => {
    if (uploading) return;
    clearSelection();
    message('已取消，本次选择未保存。', 'pending');
  });

  form.addEventListener('submit', async event => {
    event.preventDefault();
    if (uploading || !selected) return;
    uploading = true;
    choose.disabled = input.disabled = save.disabled = cancel.disabled = true;
    form.setAttribute('aria-busy', 'true');
    save.textContent = '正在上传…';
    message('正在上传头像，请稍候…', 'pending');
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 15000);
    try {
      const response = await fetch('/api/avatar', {
        method: 'POST', credentials: 'same-origin',
        headers: { 'Content-Type': 'image/png', 'Accept': 'application/json' },
        body: selected, signal: controller.signal
      });
      if (JiaoSession.redirectIfUnauthorized(response)) return;
      const data = await response.json();
      if (!response.ok || !data || data.success !== true) {
        throw new Error(data && typeof data.message === 'string' ? data.message : '头像上传失败，请重试。');
      }
      clearTimeout(timeout);
      // 上传接口没有返回图片地址，通过现有个人信息接口重新获取。
      const user = await JiaoSession.init({ required: true, endpoint: '/api/showuser' });
      if (user) {
        currentUser = user;
        clearSelection();
        message('头像已更新，继续今天的学习吧。', 'success');
      } else {
        selected = null;
        actions.hidden = true;
        message('头像已上传，暂时无法刷新资料，请稍后刷新页面。', 'success');
      }
    } catch (error) {
      const text = error.name === 'AbortError' ? '上传超时，头像可能已保存。请先刷新信息确认，再决定是否重试。'
        : error.name === 'TypeError' ? '网络连接失败，请检查网络后重试。'
        : error instanceof SyntaxError ? '服务响应异常，请稍后重试。' : error.message;
      message(text, 'error');
    } finally {
      clearTimeout(timeout);
      uploading = false;
      choose.disabled = input.disabled = cancel.disabled = false;
      save.disabled = !selected;
      save.textContent = '保存头像';
      form.setAttribute('aria-busy', 'false');
    }
  });

  window.addEventListener('pagehide', () => {
    if (previewUrl) URL.revokeObjectURL(previewUrl);
  });

  return { setUser };
})();
