'use strict';

window.JiaoProgress = (() => {
  let saving = false;
  let saveBound = false;

  function markChanged() {
    const status = document.getElementById('save-status');
    if (!status || saving) return;
    status.textContent = '有新的作答记录，请点击“保存记录”。';
    status.dataset.state = 'pending';
    status.hidden = false;
  }

  function bindSaveButton({ onSaved, canSave = () => true, onBusy = () => {} } = {}) {
    const button = document.getElementById('save-progress');
    const status = document.getElementById('save-status');
    if (!button || !status || saveBound) return;
    saveBound = true;
    button.addEventListener('click', async () => {
      if (saving || !canSave()) return;
      const logout = document.getElementById('logout-button');
      if (logout && logout.disabled) return;
      const controller = new AbortController();
      const timeout = setTimeout(() => controller.abort(), 12000);
      let saved = false;
      saving = true;
      button.disabled = true;
      button.textContent = '正在保存…';
      if (logout) logout.disabled = true;
      status.textContent = '正在保存作答记录，请稍候…';
      status.dataset.state = 'pending';
      status.hidden = false;
      button.setAttribute('aria-busy', 'true');
      try {
        onBusy(true);
        const response = await fetch('/api/progress/save', {
          method: 'POST', headers: { 'Accept': 'application/json' },
          credentials: 'same-origin', cache: 'no-store', signal: controller.signal
        });
        if (JiaoSession.redirectIfUnauthorized(response)) return;
        let data;
        try { data = await response.json(); }
        catch (error) {
          if (error.name === 'AbortError') throw error;
          throw new Error('保存结果无法确认，请稍后重试。');
        }
        if (!response.ok || !data || data.success !== true) {
          throw new Error(data && typeof data.message === 'string' && data.message
            ? data.message : '保存失败，请重试。');
        }
        if (!Number.isSafeInteger(data.saved_count) || data.saved_count < 0) {
          throw new Error('保存结果格式异常，请稍后重试确认。');
        }
        saved = true;
        status.dataset.state = 'success';
        status.textContent = data.saved_count === 0
          ? '没有需要保存的修改。' : `已保存 ${data.saved_count} 道题的作答记录。`;
      } catch (error) {
        status.dataset.state = 'error';
        status.textContent = error.name === 'AbortError'
          ? '保存请求超时，结果尚未确认。请检查网络后重新点击保存。'
          : (error.name === 'TypeError' ? '暂时无法连接服务，请检查网络后重试。' : error.message);
      } finally {
        clearTimeout(timeout);
        saving = false;
        button.disabled = false;
        button.textContent = '保存记录';
        button.setAttribute('aria-busy', 'false');
        if (logout) logout.disabled = false;
        onBusy(false);
      }
      if (saved && onSaved) {
        try { await onSaved(); }
        catch (_) {
          // 数据库保存已经成功，页面刷新失败不能被描述成保存失败。
          status.textContent += ' 页面刷新未完成，可手动刷新查看。';
        }
      }
    });
  }

  function formatTime(value) {
    if (value === undefined || value === null || value === '' || Number(value) === 0) return '时间未记录';
    if (typeof value === 'string' && /^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/.test(value)) return value;
    const seconds = Number(value);
    if (!Number.isFinite(seconds) || seconds <= 0) return '时间未记录';
    const date = new Date(seconds * 1000);
    if (Number.isNaN(date.getTime())) return '时间未记录';
    return new Intl.DateTimeFormat('zh-CN', {
      year: 'numeric', month: '2-digit', day: '2-digit',
      hour: '2-digit', minute: '2-digit', hour12: false
    }).format(date);
  }

  function normalizeQuestions(questions) {
    if (!Array.isArray(questions)) throw new Error('题目数据格式异常，请稍后重试。');
    return questions.map(question => {
      if (!question || !Number.isInteger(question.id) || typeof question.title !== 'string' ||
          !['string', 'number'].includes(typeof question.grade)) {
        throw new Error('题目数据格式异常，请稍后重试。');
      }
      const attempt = question.attempt ?? question.attempt_count ?? 0;
      const correctCount = question.correct_count ?? 0;
      if (!Number.isSafeInteger(attempt) || attempt < 0 || !Number.isSafeInteger(correctCount) ||
          correctCount < 0 || correctCount > attempt ||
          (question.last_answer != null && typeof question.last_answer !== 'string')) {
        throw new Error('作答记录格式异常，请刷新后重试。');
      }
      let lastCorrect = typeof question.last_correct === 'boolean' ? question.last_correct : null;
      if (lastCorrect === null && question.status === '最近答对') lastCorrect = true;
      if (lastCorrect === null && question.status === '最近答错') lastCorrect = false;
      const state = attempt === 0 ? 'unattempted'
        : (lastCorrect === true ? 'correct' : (lastCorrect === false ? 'incorrect' : 'attempted'));
      const status = { unattempted: '未作答', correct: '最近答对', incorrect: '最近答错', attempted: '已作答' }[state];
      return { ...question, attempt, correctCount, state, status,
        lastAnswer: attempt ? (question.last_answer || '') : '',
        lastTime: attempt ? formatTime(question.last_answer_at) : '—' };
    });
  }

  function summarize(questions) {
    const submissions = questions.reduce((sum, q) => sum + q.attempt, 0);
    const correct = questions.reduce((sum, q) => sum + q.correctCount, 0);
    return {
      attempted: questions.filter(q => q.attempt > 0).length,
      total: questions.length,
      submissions,
      accuracy: submissions ? `${Math.round(correct / submissions * 100)}%` : '—',
      incorrect: questions.filter(q => q.state === 'incorrect').length
    };
  }

  async function fetchQuestions() {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 12000);
    try {
      const response = await fetch('/api/allquestions', {
        headers: { 'Accept': 'application/json' },
        credentials: 'same-origin', cache: 'no-store', signal: controller.signal
      });
      if (JiaoSession.redirectIfUnauthorized(response)) return null;
      let data;
      try { data = await response.json(); }
      catch (error) {
        if (error.name === 'AbortError') throw error;
        throw new Error('服务响应异常，请稍后重试。');
      }
      if (!response.ok || !data || data.success !== true) {
        throw new Error(data && typeof data.message === 'string' ? data.message : '获取作答记录失败。');
      }
      return normalizeQuestions(data.questions);
    } finally {
      clearTimeout(timeout);
    }
  }

  return { normalizeQuestions, summarize, fetchQuestions, bindSaveButton, markChanged,
    get saving() { return saving; } };
})();
