'use strict';

// 运行真实页面脚本，使用最小 DOM 和可控 HTTP 响应验证交互。
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const root = path.join(__dirname, '../wwwroot');
const response = (status, data) => ({ status, ok: status >= 200 && status < 300,
  json: async () => data });
const question = { id: 1, title: '<img src=x onerror=alert(1)> 比较运算符', choice: 'A. ==\nB. =', grade: '5' };
const user = { id: 42, idcount: 'student', name: '<同学>', identity: 'student' };

class Element {
  constructor(attrs = {}) {
    this.attrs = attrs; this.children = []; this.events = {}; this.dataset = {};
    this.hidden = 'hidden' in attrs; this.disabled = 'disabled' in attrs;
    this.type = attrs.type || ''; this.value = ''; this._text = ''; this.validation = '';
  }
  set textContent(value) { this._text = String(value); this.children = []; }
  get textContent() { return this._text + this.children.map(c => c.textContent).join(''); }
  setAttribute(key, value) { this.attrs[key] = String(value); }
  removeAttribute(key) { delete this.attrs[key]; }
  contains(target) { return this === target || this.children.some(child => child.contains(target)); }
  addEventListener(name, callback) { (this.events[name] ||= []).push(callback); }
  append(...children) {
    for (const child of children) this.children.push(...(child.fragment ? child.children : [child]));
  }
  replaceChildren(...children) { this.children = []; this.append(...children); }
  focus() { this.focused = true; }
  setCustomValidity(value) { this.validation = value; }
  reportValidity() { return true; }
  async emit(name, event = {}) {
    for (const callback of this.events[name] || []) await callback({ target: this, preventDefault() {}, ...event });
  }
}

const settle = async () => { for (let i = 0; i < 6; ++i) await new Promise(setImmediate); };

async function openPage(file, overrides = {}) {
  const html = fs.readFileSync(path.join(root, file), 'utf8');
  const elements = new Map();
  const stack = [];
  const voidTags = new Set(['area', 'base', 'br', 'col', 'embed', 'hr', 'img', 'input', 'link', 'meta', 'param', 'source', 'track', 'wbr']);
  for (const [, closing, name, tag] of html.matchAll(/<(\/?)([a-z][a-z0-9-]*)([^>]*?)>/gi)) {
    if (closing) {
      const index = stack.map(entry => entry.name).lastIndexOf(name);
      if (index !== -1) stack.length = index;
      continue;
    }
    const attrs = {};
    for (const [, key, value] of tag.matchAll(/([\w-]+)="([^"]*)"/g)) attrs[key] = value;
    if (/\shidden(?:\s|$)/.test(tag)) attrs.hidden = '';
    if (/\sdisabled(?:\s|$)/.test(tag)) attrs.disabled = '';
    const node = new Element(attrs);
    if (attrs.id) {
      assert(!elements.has(attrs.id), `duplicate ID: ${attrs.id}`);
      elements.set(attrs.id, node);
    }
    if (stack.length) stack[stack.length - 1].node.append(node);
    if (!voidTags.has(name)) stack.push({ name, node });
  }
  if (elements.has('register-identity')) elements.get('register-identity').value = 'student';
  const calls = [], navigations = [], windowEvents = {}, documentEvents = {};
  const sandbox = {
    URLSearchParams, AbortController, clearTimeout, console,
    setTimeout: (callback, delay) => {
      if (overrides.timers) overrides.timers.push(callback);
      return setTimeout(callback, delay);
    },
    location: { search: overrides.search || '', hash: overrides.hash || '',
      assign: url => navigations.push(url), replace: url => navigations.push(url),
      reload: () => navigations.push('reload') },
    addEventListener: (name, fn) => { (windowEvents[name] ||= []).push(fn); },
    document: {
      getElementById: id => elements.get(id) || null,
      addEventListener: (name, fn) => { (documentEvents[name] ||= []).push(fn); },
      createElement: () => new Element(),
      createDocumentFragment: () => Object.assign(new Element(), { fragment: true })
    },
    fetch: async (url, options = {}) => {
      calls.push({ url, options });
      if (overrides.fetch) {
        const result = await overrides.fetch(url, options);
        if (result) return result;
      }
      if (url === '/api/me' || url === '/api/showuser') return response(200, { success: true, user });
      if (url === '/api/allquestions') return response(200, { success: true, questions: [question] });
      if (url === '/api/question/1') return response(200, { success: true, question });
      if (url.endsWith('/judge')) return response(200, { success: true, correct: true, message: '回答正确' });
      if (url === '/api/progress/save') return response(200, { success: true, saved_count: 0 });
      return response(200, { success: true, message: '成功' });
    }
  };
  sandbox.window = sandbox;
  vm.createContext(sandbox);
  for (const [, attrs, script] of html.matchAll(/<script([^>]*)>([\s\S]*?)<\/script>/g)) {
    const source = attrs.match(/src="([^"]+)"/);
    vm.runInContext(source
      ? fs.readFileSync(path.join(root, source[1].replace(/^\//, '')), 'utf8')
      : script, sandbox);
  }
  await settle();
  return { get: id => elements.get(id), calls, navigations, sandbox, windowEvents,
    emitDocument: async (name, event) => {
      for (const callback of documentEvents[name] || []) await callback({ preventDefault() {}, ...event });
    } };
}

(async () => {
  let page = await openPage('index.html', { fetch: url => url === '/api/me' && response(401, { success: false }) });
  page.get('idcount').value = 'student';
  page.get('idpasswd').value = 'test-password';
  await page.get('login-form').emit('submit');
  assert.deepEqual(page.navigations, ['/questions.html']);
  assert.equal(page.get('idpasswd').value, '');
  assert.equal(page.calls.find(c => c.url === '/api/login').options.body,
    JSON.stringify({ idcount: 'student', idpasswd: 'test-password' }));

  page = await openPage('index.html', { search: '?reason=expired',
    fetch: url => url === '/api/login' && response(401, { success: false, message: '账号或密码错误' }) });
  assert(page.get('login-status').textContent.includes('登录已失效'));
  page.get('idcount').value = 'student'; page.get('idpasswd').value = 'bad';
  await page.get('login-form').emit('submit');
  assert.equal(page.navigations.length, 0);
  assert.equal(page.get('login-status').textContent, '账号或密码错误');
  assert(!page.get('submit-button').disabled);
  page.sandbox.location.hash = '#register';
  for (const callback of page.windowEvents.hashchange) callback();
  page.get('register-name').value = ' 新同学 ';
  page.get('register-account').value = 'new';
  page.get('register-password').value = page.get('register-confirm').value = 'password';
  await page.get('register-form').emit('submit');
  const registration = JSON.parse(page.calls.find(c => c.url === '/api/register').options.body);
  assert.deepEqual(registration, { name: '新同学', idcount: 'new', idpasswd: 'password', identity: 'student' });
  assert.equal(page.get('register-password').value, '');
  assert.equal(page.get('idcount').value, 'new');

  page = await openPage('questions.html');
  assert.equal(page.get('session-name').textContent, '<同学>');
  assert(!page.get('session-info').hidden);
  assert.equal(page.get('user-link').attrs.href, '/user.html');
  assert.equal(page.get('profile-panel'), undefined);
  const titleLink = page.get('question-rows').children[0].children[1].children[0];
  assert.equal(titleLink.textContent, question.title);
  assert.equal(titleLink.href, '/question.html?id=1');
  page.get('question-search').value = '不存在';
  await page.get('question-search').emit('input');
  assert(page.get('list-message').textContent.includes('没有匹配'));
  page.get('question-search').value = '1';
  await page.get('question-search').emit('input');
  assert.equal(page.get('question-rows').children.length, 1);
  await page.get('logout-button').emit('click');
  assert.deepEqual(page.navigations, ['/index.html?reason=logout#login']);
  assert.equal(page.calls.find(c => c.url === '/api/logout').options.method, 'POST');

  page = await openPage('questions.html', { fetch: url =>
    ['/api/me', '/api/allquestions'].includes(url) && response(401, { success: false }) });
  assert.deepEqual(page.navigations, ['/index.html?reason=expired#login']);
  assert.equal(page.get('question-rows').children.length, 0);

  let verdict = false;
  page = await openPage('question.html', { search: '?id=1', fetch: url =>
    url.endsWith('/judge') && response(200, { success: true, correct: verdict, message: verdict ? '回答正确' : '回答错误' }) });
  assert(!page.get('answer-fields').disabled);
  page.get('answer-input').value = ' B ';
  await page.get('answer-form').emit('submit');
  assert.equal(page.get('answer-result').dataset.state, 'incorrect');
  assert.equal(page.calls.find(c => c.url.endsWith('/judge')).options.body, '{"answer":"B"}');
  verdict = true;
  page.get('answer-input').value = 'a';
  await page.get('answer-form').emit('submit');
  assert.equal(page.get('answer-result').dataset.state, 'correct');
  page.get('answer-input').value = '   ';
  const before = page.calls.length;
  await page.get('answer-form').emit('submit');
  assert.equal(page.calls.length, before);

  page = await openPage('question.html', { search: '?id=1', fetch: url =>
    url.endsWith('/judge') && response(401, { success: false }) });
  page.get('answer-input').value = 'A';
  await page.get('answer-form').emit('submit');
  assert.deepEqual(page.navigations, ['/index.html?reason=expired#login']);

  page = await openPage('questions.html', { fetch: url => {
    if (url === '/api/logout') throw new Error('网络中断');
  } });
  await page.get('logout-button').emit('click');
  assert.equal(page.navigations.length, 0);
  assert(!page.get('logout-button').disabled);
  assert.equal(page.get('session-notice').textContent, '网络中断');
  for (const callback of page.windowEvents.pageshow) callback({ persisted: true });
  assert.deepEqual(page.navigations, ['reload']);

  page = await openPage('index.html');
  assert(!page.get('active-session').hidden);
  assert.equal(page.get('user-link').attrs.href, '/user.html');
  page = await openPage('question.html', { search: '?id=1' });
  assert.equal(page.get('user-link').attrs.href, '/user.html');

  page = await openPage('user.html');
  assert.deepEqual(page.calls.map(call => call.url), ['/api/showuser', '/api/allquestions']);
  assert(!page.get('user-detail').hidden && page.get('user-message').hidden);
  assert.equal(page.get('user-display-name').textContent, '<同学>');
  assert.equal(page.get('user-account').textContent, 'student');
  assert.equal(page.get('user-identity').textContent, '学生');
  await page.get('refresh-user').emit('click');
  assert.equal(page.calls.filter(call => call.url === '/api/showuser').length, 2);
  await page.get('logout-button').emit('click');
  assert.equal(page.calls.filter(call => call.url === '/api/logout').length, 1);
  assert.deepEqual(page.navigations, ['/index.html?reason=logout#login']);

  let profileFailure = true;
  page = await openPage('user.html', { fetch: url => {
    if (url === '/api/showuser' && profileFailure) throw new Error('获取失败');
  } });
  assert(page.get('user-detail').hidden && !page.get('refresh-user').disabled);
  assert.equal(page.get('session-notice').textContent, '获取失败');
  assert(page.get('study-message').textContent.includes('尚未加载'));
  profileFailure = false;
  await page.get('refresh-user').emit('click');
  assert(!page.get('user-detail').hidden && page.get('session-notice').hidden);

  page = await openPage('user.html', { fetch: url =>
    url === '/api/showuser' && response(401, { success: false }) });
  assert.deepEqual(page.navigations, ['/index.html?reason=expired#login']);
  assert(page.get('user-detail').hidden);
  page = await openPage('user.html', { fetch: url =>
    url === '/api/showuser' && response(200, { success: true, user: { ...user, name: '', identity: '老师' } }) });
  assert.equal(page.get('user-display-name').textContent, 'student');
  assert.equal(page.get('user-identity').textContent, '教师');
  page = await openPage('user.html', { fetch: url =>
    url === '/api/showuser' && response(200, { success: true, user: { ...user, name: {} } }) });
  assert(page.get('user-detail').hidden && !page.get('refresh-user').disabled);
  const progressQuestions = [
    { ...question, attempt: 0, correct_count: 0, last_answer: '', status: '未作答' },
    { ...question, id: 2, attempt: 3, correct_count: 2, last_answer: '<B>', last_correct: true, last_answer_at: 1729000000 },
    { ...question, id: 3, attempt: 2, correct_count: 0, last_answer: 'A', last_correct: false, last_answer_at: 0 },
    { ...question, id: 4, attempt: 1, correct_count: 1, last_answer: 'C' },
    { ...question, id: 5, attempt: 0, correct_count: 0, last_correct: false }
  ];
  const progressFetch = url => url === '/api/allquestions' && response(200, { success: true, questions: progressQuestions });
  page = await openPage('questions.html', { fetch: progressFetch });
  assert.equal(page.get('question-rows').children.length, 5);
  assert.equal(page.get('summary-attempted').textContent, '3 / 5');
  assert.equal(page.get('summary-submissions').textContent, '6');
  assert.equal(page.get('summary-accuracy').textContent, '50%');
  assert.equal(page.get('summary-incorrect').textContent, '1');
  assert.equal(page.get('question-rows').children[0].children[2].textContent, '未作答');
  assert.equal(page.get('question-rows').children[1].children[2].textContent, '最近答对');
  assert(page.get('question-rows').children[1].children[1].textContent.includes('<B>'));
  assert(page.get('question-rows').children[1].children[1].textContent.includes('2024'));
  assert(page.get('question-rows').children[2].children[1].textContent.includes('时间未记录'));
  assert.equal(page.get('question-rows').children[3].children[2].textContent, '已作答');
  assert.equal(page.get('question-rows').children[4].children[2].textContent, '未作答');
  page.get('status-filter').value = 'incorrect';
  await page.get('status-filter').emit('change');
  assert.equal(page.get('question-rows').children.length, 1);
  assert.equal(page.get('question-rows').children[0].children[0].textContent, '3');
  page.get('question-search').value = '不存在';
  await page.get('question-search').emit('input');
  assert(page.get('list-message').textContent.includes('没有匹配'));
  page = await openPage('questions.html', { search: '?status=incorrect', fetch: progressFetch });
  assert.equal(page.get('question-rows').children.length, 1);
  assert.equal(page.get('question-rows').children[0].children[0].textContent, '3');
  page = await openPage('questions.html', { fetch: url => url === '/api/allquestions' &&
    response(200, { success: true, questions: [] }) });
  assert.equal(page.get('summary-attempted').textContent, '0 / 0');
  assert.equal(page.get('summary-accuracy').textContent, '—');
  assert(page.get('list-message').textContent.includes('还没有题目'));
  page = await openPage('questions.html', { fetch: url => url === '/api/allquestions' &&
    response(200, { success: true, questions: [{ ...question, attempt: -1 }] }) });
  assert(page.get('question-table').hidden && page.get('progress-summary').hidden);
  assert(page.get('status-filter').disabled && !page.get('refresh-button').disabled);
  assert(page.get('list-message').textContent.includes('格式异常'));

  page = await openPage('user.html', { fetch: progressFetch });
  assert.equal(page.get('study-attempted').textContent, '3 / 5');
  assert.equal(page.get('study-accuracy').textContent, '50%');
  assert(!page.get('user-detail').hidden && !page.get('study-summary').hidden);
  page = await openPage('user.html', { fetch: url => {
    if (url === '/api/allquestions') throw new Error('记录暂不可用');
  } });
  assert(!page.get('user-detail').hidden && page.get('study-summary').hidden);
  assert(!page.get('refresh-study').disabled);

  let attempts = 0;
  page = await openPage('question.html', { search: '?id=1', fetch: (url, options) => {
    if (url === '/api/allquestions') return response(200, { success: true, questions: [
      { ...question, attempt: attempts, correct_count: attempts, last_answer: attempts ? 'A' : '', last_correct: attempts ? true : undefined }
    ] });
    if (url.endsWith('/judge')) {
      ++attempts;
      return response(200, { success: true, correct: true, message: '回答正确' });
    }
  } });
  assert.equal(page.get('practice-status').textContent, '未作答');
  page.get('answer-input').value = 'A';
  await page.get('answer-form').emit('submit');
  await settle();
  assert.equal(page.get('practice-attempt').textContent, '1 次');
  assert.equal(page.get('practice-correct').textContent, '1 次');
  assert.equal(page.get('practice-status').textContent, '最近答对');
  assert(page.get('save-status').textContent.includes('新的作答记录'));
  assert.equal(page.get('practice-answer').textContent, 'A');
  assert.equal(page.get('practice-time').textContent, '时间未记录');

  let failRecords = false;
  page = await openPage('question.html', { search: '?id=1', fetch: url => {
    if (url === '/api/allquestions' && failRecords) throw new Error('刷新失败');
  } });
  failRecords = true;
  page.get('answer-input').value = 'A';
  await page.get('answer-form').emit('submit');
  await settle();
  assert.equal(page.get('answer-result').dataset.state, 'correct');
  assert.equal(page.get('practice-message').textContent, '刷新失败');
  assert(!page.get('refresh-progress').disabled);
  failRecords = false;
  await page.get('refresh-progress').emit('click');
  assert(page.get('practice-message').hidden);
  for (const file of ['questions.html', 'question.html', 'user.html']) {
    page = await openPage(file, { search: '?id=1', fetch: url => url === '/api/progress/save' &&
      response(200, { success: true, saved_count: 2 }) });
    const beforeRefresh = page.calls.filter(call => call.url === '/api/allquestions').length;
    await page.get('save-progress').emit('click');
    const call = page.calls.find(call => call.url === '/api/progress/save');
    assert.equal(call.options.method, 'POST');
    assert.equal(call.options.credentials, 'same-origin');
    assert.equal(call.options.body, undefined);
    assert.equal(page.get('save-status').dataset.state, 'success');
    assert(page.get('save-status').textContent.includes('2 道题'));
    assert(!page.get('save-progress').disabled && !page.get('logout-button').disabled);
    assert.equal(page.calls.filter(call => call.url === '/api/allquestions').length, beforeRefresh + 1);
  }
  page = await openPage('questions.html');
  await page.get('save-progress').emit('click');
  assert(page.get('save-status').textContent.includes('没有需要保存'));

  let resolveSave;
  page = await openPage('question.html', { search: '?id=1', fetch: url => {
    if (url === '/api/progress/save') return new Promise(resolve => { resolveSave = resolve; });
  } });
  const pendingSave = page.get('save-progress').emit('click');
  await settle();
  assert(page.get('save-progress').disabled && page.get('logout-button').disabled);
  assert(page.get('answer-fields').disabled);
  await page.get('save-progress').emit('click');
  page.get('answer-input').value = 'A';
  await page.get('answer-form').emit('submit');
  assert.equal(page.calls.filter(call => call.url === '/api/progress/save').length, 1);
  assert.equal(page.calls.filter(call => call.url.endsWith('/judge')).length, 0);
  resolveSave(response(200, { success: true, saved_count: 1 }));
  await pendingSave;
  assert(!page.get('answer-fields').disabled && !page.get('save-progress').disabled);

  let saveFails = true;
  page = await openPage('questions.html', { fetch: url => url === '/api/progress/save' &&
    response(saveFails ? 500 : 200, saveFails ? { success: false, message: '数据库暂不可用' }
      : { success: true, saved_count: 1 }) });
  await page.get('save-progress').emit('click');
  assert.equal(page.get('save-status').dataset.state, 'error');
  assert.equal(page.get('save-status').textContent, '数据库暂不可用');
  assert(!page.get('save-progress').disabled);
  saveFails = false;
  await page.get('save-progress').emit('click');
  assert.equal(page.get('save-status').dataset.state, 'success');

  page = await openPage('questions.html', { fetch: url => url === '/api/progress/save' &&
    response(401, { success: false }) });
  await page.get('save-progress').emit('click');
  assert.deepEqual(page.navigations, ['/index.html?reason=expired#login']);
  assert.notEqual(page.get('save-status').dataset.state, 'success');
  page = await openPage('questions.html', { fetch: url => url === '/api/progress/save' &&
    response(200, { success: true }) });
  await page.get('save-progress').emit('click');
  assert.equal(page.get('save-status').dataset.state, 'error');
  assert(page.get('save-status').textContent.includes('格式异常'));

  const timers = [];
  page = await openPage('questions.html', { timers, fetch: (url, options) => {
    if (url === '/api/progress/save') return new Promise((_, reject) => {
      options.signal.addEventListener('abort', () => reject(Object.assign(new Error('timeout'), { name: 'AbortError' })));
    });
  } });
  const timedSave = page.get('save-progress').emit('click');
  await settle();
  timers[timers.length - 1]();
  await timedSave;
  assert(page.get('save-status').textContent.includes('结果尚未确认'));
  assert(!page.get('save-progress').disabled && !page.get('logout-button').disabled);
  console.log('Frontend: progress display, save buttons, duplicate protection, retry, timeout and auth passed');
})().catch(error => { console.error(error); process.exitCode = 1; });
