const $ = (selector) => document.querySelector(selector);
let csrf = sessionStorage.getItem('nexo_csrf') || '';
let challenge = '';
let pendingEmail = '';

async function authApi(path, init = {}) {
  const headers = new Headers(init.headers);
  headers.set('Content-Type', 'application/json');
  if (csrf) headers.set('X-CSRF-Token', csrf);
  const response = await fetch(path, { ...init, headers });
  const value = await response.json().catch(() => ({}));
  if (!response.ok) throw new Error(value.error?.message || 'Erro de autenticação');
  return value;
}

function unlock() {
  document.body.classList.remove('auth-locked');
  hydrate();
}

function showEmailStep() {
  challenge = '';
  $('#development-code strong').textContent = '';
  $('#code-form').reset();
  $('#code-form').hidden = true;
  $('#email-form').hidden = false;
}

async function authenticate() {
  try {
    await authApi('/api/v1/auth/me');
    unlock();
  } catch (_) {
    document.body.classList.add('auth-locked');
  }
}

$('#email-form').addEventListener('submit', async (event) => {
  event.preventDefault();
  const error = event.currentTarget.querySelector('.login-error');
  error.textContent = '';
  try {
    const data = Object.fromEntries(new FormData(event.currentTarget));
    pendingEmail = String(data.email).trim().toLowerCase();
    if (!/^[^@\s]+@embrapa\.br$/.test(pendingEmail)) {
      throw new Error('Use um e-mail institucional válido @embrapa.br.');
    }
    const result = await authApi('/api/v1/auth/request-code', {
      method: 'POST', body: JSON.stringify(Object.fromEntries(new FormData(event.currentTarget)))
    });
    challenge = result.challenge_id;
    $('#code-destination').textContent = `Identidade local associada a ${pendingEmail}. Código válido por 15 minutos.`;
    $('#development-code strong').textContent = result.dev_code;
    $('#email-form').hidden = true;
    $('#code-form').hidden = false;
    $('#code-form input[name="code"]').focus();
  } catch (failure) {
    error.textContent = failure.message;
  }
});

$('#resend-code').addEventListener('click', () => {
  $('#code-form').hidden = true;
  $('#email-form').hidden = false;
  $('#email-form').requestSubmit();
});

$('#code-form').addEventListener('submit', async (event) => {
  event.preventDefault();
  const error = event.currentTarget.querySelector('.login-error');
  error.textContent = '';
  try {
    const code = new FormData(event.currentTarget).get('code');
    const result = await authApi('/api/v1/auth/verify-code', {
      method: 'POST', body: JSON.stringify({ challenge_id: challenge, code })
    });
    csrf = result.csrf_token;
    sessionStorage.setItem('nexo_csrf', csrf);
    unlock();
  } catch (failure) {
    error.textContent = failure.message;
  }
});

$('#change-email').addEventListener('click', () => {
  showEmailStep();
  $('#email-form input[name="email"]').focus();
});

$('#logout').addEventListener('click', async () => {
  try { await authApi('/api/v1/auth/logout', { method: 'POST', body: '{}' }); } catch (_) { /* sessão será descartada localmente */ }
  csrf = '';
  sessionStorage.removeItem('nexo_csrf');
  showEmailStep();
  document.body.classList.add('auth-locked');
});

function updateClock() {
  const now = new Intl.DateTimeFormat('pt-BR', {
    timeZone: 'America/Sao_Paulo', hour: '2-digit', minute: '2-digit', second: '2-digit'
  }).format(new Date());
  $('#clock').textContent = `${now} BRT`;
}

async function readJson(path) {
  const response = await fetch(path, { headers: { Accept: 'application/json' } });
  if (!response.ok) throw new Error(`HTTP ${response.status}`);
  return response.json();
}

async function hydrate() {
  try {
    const [health, readiness, capabilities] = await Promise.all([
      readJson('/api/health'), readJson('/api/readiness'), readJson('/api/capabilities')
    ]);
    const online = health.status === 'ok';
    $('#nav-status').textContent = online ? 'operacional' : health.status;
    $('.pulse').classList.toggle('ok', online);
    $('#health-dot').classList.toggle('ok', online);
    $('#health-value').textContent = online ? 'Operacional' : health.status;
    $('#health-note').textContent = online
      ? 'O núcleo responde e preserva sua fronteira operacional.'
      : 'O núcleo respondeu com estado degradado.';
    $('#health-schema').textContent = health.schema;
    $('#complete-value').textContent = health.complete ? 'sim' : 'não — por princípio';
    $('#ready-value').textContent = readiness.ready ? '100%' : '0%';
    $('#ready-meter').style.width = readiness.ready ? '100%' : '0%';
    $('#capability-list').replaceChildren(...capabilities.capabilities.map((capability) => {
      const item = document.createElement('li');
      item.textContent = `${capability.id} · v${capability.version}`;
      return item;
    }));
  } catch (error) {
    $('#nav-status').textContent = 'indisponível';
    $('#health-value').textContent = 'Sem contato';
    $('#health-note').textContent = 'Não foi possível consultar as APIs locais.';
    $('#capability-list').innerHTML = '<li>API indisponível</li>';
    console.error('Falha ao consultar estado do NEXO:', error);
  }
}

updateClock();
setInterval(updateClock, 1000);
authenticate();
