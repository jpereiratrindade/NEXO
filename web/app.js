const $ = (selector) => document.querySelector(selector);
let csrf = sessionStorage.getItem('nexo_csrf') || '';
let challenge = '';
let pendingEmail = '';

async function refreshCsrf() {
  const response = await fetch('/api/v1/auth/csrf', {
    method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{}'
  });
  const value = await response.json().catch(() => ({}));
  if (!response.ok || !value.csrf_token) throw new Error(value.error?.message || 'Sessão inválida');
  csrf = value.csrf_token;
  sessionStorage.setItem('nexo_csrf', csrf);
}

async function authApi(path, init = {}, retryCsrf = true) {
  const headers = new Headers(init.headers);
  headers.set('Content-Type', 'application/json');
  if (csrf) headers.set('X-CSRF-Token', csrf);
  const response = await fetch(path, { ...init, headers });
  const value = await response.json().catch(() => ({}));
  if (!response.ok && response.status === 403 && value.error?.code === 'csrf_failed' && retryCsrf) {
    await refreshCsrf();
    return authApi(path, init, false);
  }
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
    if (!csrf) await refreshCsrf();
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

// Renderização estritamente textual dos metadados públicos recebidos do REDAPE.
function safeRedapeUrl(value) {
  try {
    const url = new URL(String(value || ''));
    return url.protocol === 'https:' ? url.href : '';
  } catch (_) {
    return '';
  }
}

function renderRedapeResult(reference) {
  const item = document.createElement('li');
  item.className = 'nexo-result-card';

  const details = document.createElement('details');
  details.className = 'nexo-result-details';
  const summary = document.createElement('summary');
  summary.className = 'nexo-result-summary';

  const title = document.createElement('strong');
  title.className = 'nexo-result-title';
  title.textContent = reference.title || 'Conjunto sem título';

  const meta = document.createElement('span');
  meta.className = 'nexo-result-meta';
  const id = document.createElement('span');
  id.textContent = reference.external_id || 'Identificador não informado';
  const version = document.createElement('span');
  version.textContent = reference.version ? `Versão ${reference.version}` : 'Versão não informada';
  const prompt = document.createElement('span');
  prompt.className = 'nexo-result-prompt';
  prompt.textContent = 'Ver detalhes';
  meta.append(id, version, prompt);
  summary.append(title, meta);

  const panel = document.createElement('div');
  panel.className = 'nexo-result-panel';
  const fields = document.createElement('dl');
  const appendField = (label, value) => {
    const term = document.createElement('dt');
    const definition = document.createElement('dd');
    term.textContent = label;
    definition.textContent = value || 'Não informado';
    fields.append(term, definition);
  };
  appendField('Fonte', 'REDAPE · Embrapa');
  appendField('Identificador', reference.external_id);
  appendField('Versão', reference.version);
  appendField('URL declarada', reference.canonical_uri);
  panel.append(fields);

  const uri = safeRedapeUrl(reference.canonical_uri);
  if (uri) {
    const link = document.createElement('a');
    link.className = 'nexo-result-open';
    link.href = uri;
    link.target = '_blank';
    link.rel = 'noopener noreferrer';
    link.textContent = 'Abrir referência original ↗';
    panel.append(link);
  }
  details.append(summary, panel);
  item.append(details);
  return item;
}

let lastSearchResults = [];

async function runRedapeQuery(persist) {
  const form = $('#redape-ingest-form');
  const output = $('#redape-result');
  const buttons = form.querySelectorAll('button');
  output.textContent = persist ? 'Catalogando referências no NEXO…' : 'Pesquisando REDAPE sem gravar…';
  buttons.forEach((button) => { button.disabled = true; });
  try {
    const payload = Object.fromEntries(new FormData(form));
    const endpoint = persist ? '/api/v1/connectors/redape/ingest' : '/api/v1/connectors/redape/search';
    const result = await authApi(endpoint, {
      method: 'POST', body: JSON.stringify(payload)
    });
    const references = Array.isArray(result.items) ? result.items : [];
    lastSearchResults = references;
    const scopeSearchOpt = $('#graph-scope-search-opt');
    if (scopeSearchOpt) {
      scopeSearchOpt.textContent = `Resultados da pesquisa REDAPE ativa (${references.length})`;
      scopeSearchOpt.disabled = references.length < 2;
    }
    output.textContent = persist
      ? `${result.cataloged} referência(s) catalogada(s); ${result.total_available} disponível(is) no REDAPE.`
      : (references.length > 0
          ? `${references.length} de ${result.total_available} referência(s) recuperada(s). Nenhuma referência catalogada.`
          : 'Nenhum conjunto de dados encontrado no REDAPE para estes termos.');
    if (references.length > 0) {
      $('#redape-results').replaceChildren(...references.map(renderRedapeResult));
    } else {
      const emptyItem = document.createElement('li');
      emptyItem.className = 'nexo-result-empty';
      emptyItem.textContent = 'Nenhum registro encontrado no REDAPE para a consulta informada. O repositório da Embrapa não possui conjuntos de dados com esta expressão exata.';
      $('#redape-results').replaceChildren(emptyItem);
    }
  } catch (failure) {
    output.textContent = failure.message;
    $('#redape-results').replaceChildren();
  } finally {
    buttons.forEach((button) => { button.disabled = false; });
  }
}

$('#redape-ingest-form').addEventListener('submit', (event) => {
  event.preventDefault();
  void runRedapeQuery(false);
});

$('#redape-catalog-button').addEventListener('click', () => {
  void runRedapeQuery(true);
});

const scopeSelect = $('#graph-scope-select');
const scopeQueryInput = $('#graph-scope-query');
if (scopeSelect && scopeQueryInput) {
  scopeSelect.addEventListener('change', () => {
    scopeQueryInput.hidden = scopeSelect.value !== 'filter';
    if (!scopeQueryInput.hidden) scopeQueryInput.focus();
  });
}

// NEXO-PATCH-003 / NEXO-PATCH-004: exploração científica do grafo
const GRAPH_SVG_NS = 'http://www.w3.org/2000/svg';
const graphElement = (tag, attrs = {}, text = '') => {
  const element = document.createElementNS(GRAPH_SVG_NS, tag);
  for (const [key, value] of Object.entries(attrs)) element.setAttribute(key, String(value));
  if (text) element.textContent = text;
  return element;
};

let currentGraphAnalysis = null;
let graphSelectionHandler = null;

function renderGraphNetwork(analysis) {
  currentGraphAnalysis = analysis;
  const container = $('#graph-map');
  const canvas = $('#graph-canvas');
  const details = $('#graph-node-details');
  const filterChips = $('#graph-community-filter');
  canvas.replaceChildren();
  details.replaceChildren();
  if (filterChips) filterChips.replaceChildren();

  const nodes = Array.isArray(analysis.nodes) ? analysis.nodes : [];
  const edges = Array.isArray(analysis.edges) ? analysis.edges : [];
  if (!nodes.length) {
    container.hidden = false;
    details.textContent = 'Nenhuma referência disponível para o escopo selecionado.';
    return;
  }

  const groups = new Map();
  const nodeById = new Map();
  for (const node of nodes) {
    const id = String(node.id);
    nodeById.set(id, node);
    const key = String(node.community);
    if (!groups.has(key)) groups.set(key, []);
    groups.get(key).push(node);
  }
  const communitiesMap = new Map();
  if (Array.isArray(analysis.communities)) {
    for (const c of analysis.communities) communitiesMap.set(String(c.id), c);
  }

  const keys = [...groups.keys()].sort((a, b) => Number(a) - Number(b));
  const columns = Math.max(1, Math.ceil(Math.sqrt(keys.length * 1.4)));
  const rows = Math.ceil(keys.length / columns);
  const cellWidth = 180;
  const cellHeight = 170;
  const width = columns * cellWidth;
  const height = rows * cellHeight;
  const positions = new Map();
  const svg = graphElement('svg', {
    viewBox: `0 0 ${width} ${height}`,
    role: 'img',
    'aria-label': `Grafo de ${nodes.length} referências e ${edges.length} relações lexicais candidatas`
  });
  const layerAreas = graphElement('g', { class: 'layer-areas' });
  const layerEdges = graphElement('g', { class: 'layer-edges' });
  const layerNodes = graphElement('g', { class: 'layer-nodes' });
  const accents = ['#008da3', '#b96519', '#426d9b', '#52786a', '#8166a2', '#a75063'];

  // Toolbar community chips
  if (filterChips) {
    const resetChip = document.createElement('button');
    resetChip.type = 'button';
    resetChip.className = 'graph-chip is-active';
    resetChip.textContent = 'Todas';
    resetChip.onclick = () => clearSelection();
    filterChips.append(resetChip);

    keys.forEach((key) => {
      const chip = document.createElement('button');
      chip.type = 'button';
      chip.className = 'graph-chip';
      chip.dataset.community = key;
      chip.textContent = `C${key}`;
      chip.title = `Comunidade C${key} (${groups.get(key).length} referências)`;
      chip.onclick = () => selectCommunity(key);
      filterChips.append(chip);
    });
  }

  keys.forEach((key, groupIndex) => {
    const members = groups.get(key);
    const cx = (groupIndex % columns + 0.5) * cellWidth;
    const cy = (Math.floor(groupIndex / columns) + 0.5) * cellHeight + 8;
    const radius = Math.min(68, 26 + 10 * Math.sqrt(members.length));
    const hue = accents[groupIndex % accents.length];
    
    const zone = graphElement('circle', {
      cx, cy, r: radius, class: 'graph-community-zone', stroke: hue,
      'data-community': key, role: 'button', tabindex: 0,
      'aria-label': `Comunidade C${key} com ${members.length} referências`
    });
    zone.onclick = () => selectCommunity(key);
    zone.onkeydown = (e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); selectCommunity(key); } };
    layerAreas.append(zone);

    const label = graphElement('text', {
      x: cx, y: cy - radius - 8, 'text-anchor': 'middle', class: 'graph-community-label',
      'data-community': key, role: 'button', tabindex: 0
    }, `C${key} · ${members.length}`);
    label.onclick = () => selectCommunity(key);
    layerAreas.append(label);

    members.forEach((node, memberIndex) => {
      const count = members.length;
      const orbit = count === 1 ? 0 : Math.min(radius - 12, 14 + Math.sqrt(count) * 7);
      const angle = (2 * Math.PI * memberIndex / count) - Math.PI / 2;
      positions.set(String(node.id), {
        x: cx + Math.cos(angle) * orbit,
        y: cy + Math.sin(angle) * orbit,
        hue
      });
    });
  });

  const edgeElements = [];
  const nodeElements = new Map();

  const relationships = edges.filter(edge => positions.has(String(edge.source)) &&
    positions.has(String(edge.target)) && edge.source !== edge.target);

  for (const edge of relationships) {
    const sId = String(edge.source);
    const tId = String(edge.target);
    const first = positions.get(sId);
    const second = positions.get(tId);
    const sNode = nodeById.get(sId);
    const tNode = nodeById.get(tId);
    const evidenceStr = Array.isArray(edge.evidence) ? edge.evidence.join(', ') : '';

    const line = graphElement('line', {
      x1: first.x, y1: first.y, x2: second.x, y2: second.y,
      class: 'graph-relationship',
      'stroke-width': 0.8 + Math.min(1, Math.max(0, Number(edge.weight) || 0)) * 2.5,
      'data-source': sId,
      'data-target': tId,
      tabindex: 0,
      role: 'button',
      'aria-label': `Ligação entre ${sNode?.title || sId} e ${tNode?.title || tId}`
    });
    line.append(graphElement('title', {},
      `${sNode?.title || sId} ↔ ${tNode?.title || tId}\nSimilaridade: ${Number(edge.weight).toFixed(2)}\nTermos compartilhados: ${evidenceStr}`));
    line.onclick = () => selectEdge(edge);
    line.onkeydown = (e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); selectEdge(edge); } };
    layerEdges.append(line);
    edgeElements.push({ element: line, edge, sId, tId });
  }

  nodes.forEach((node, index) => {
    const id = String(node.id);
    const pos = positions.get(id);
    const point = graphElement('circle', {
      cx: pos.x, cy: pos.y, r: 7.5, fill: pos.hue,
      class: 'graph-reference-node', tabindex: 0,
      'data-node-id': id,
      'data-node-index': index,
      role: 'button',
      'aria-label': `Inspecionar ${node.title || id}`
    });
    point.append(graphElement('title', {}, node.title || id));
    point.onclick = () => selectNode(id);
    point.onkeydown = (e) => {
      if (e.key === 'Enter' || e.key === ' ') {
        e.preventDefault();
        selectNode(id);
      }
    };
    layerNodes.append(point);
    nodeElements.set(id, point);
  });

  svg.append(layerAreas, layerEdges, layerNodes);
  canvas.append(svg);
  container.hidden = false;

  function clearSelection() {
    svg.querySelectorAll('.is-selected, .is-neighbor, .is-dimmed, .is-community-selected, .is-active-edge, .is-hidden-edge')
      .forEach(el => el.classList.remove('is-selected', 'is-neighbor', 'is-dimmed', 'is-community-selected', 'is-active-edge', 'is-hidden-edge'));
    if (filterChips) {
      filterChips.querySelectorAll('.graph-chip').forEach(c => c.classList.remove('is-active'));
      filterChips.querySelector('.graph-chip')?.classList.add('is-active');
    }
    details.replaceChildren();
    const prompt = document.createElement('p');
    prompt.textContent = 'Selecione um nó, aresta ou comunidade para explorar sua estrutura, pontes e evidências lexicais.';
    details.append(prompt);
  }

  function selectCommunity(communityKey) {
    clearSelection();
    const key = String(communityKey);
    const members = groups.get(key) || [];
    const commData = communitiesMap.get(key);

    if (filterChips) {
      filterChips.querySelectorAll('.graph-chip').forEach(c => {
        c.classList.toggle('is-active', c.dataset.community === key);
      });
    }

    svg.querySelectorAll('.graph-community-zone').forEach(z => {
      if (z.dataset.community === key) z.classList.add('is-community-selected');
      else z.classList.add('is-dimmed');
    });
    svg.querySelectorAll('.graph-community-label').forEach(l => {
      if (l.dataset.community !== key) l.classList.add('is-dimmed');
    });

    const memberSet = new Set(members.map(m => String(m.id)));
    nodeElements.forEach((el, id) => {
      if (!memberSet.has(id)) el.classList.add('is-dimmed');
    });

    edgeElements.forEach(({ element, sId, tId }) => {
      const sIn = memberSet.has(sId);
      const tIn = memberSet.has(tId);
      if (sIn && tIn) {
        // internal edge
      } else if (sIn || tIn) {
        element.classList.add('is-active-edge'); // bridge edge!
      } else {
        element.classList.add('is-hidden-edge');
      }
    });

    details.replaceChildren();
    const head = document.createElement('div');
    head.className = 'graph-details-header';
    const title = document.createElement('strong');
    title.textContent = `Comunidade C${key} · ${members.length} referências`;
    const clearBtn = document.createElement('button');
    clearBtn.className = 'graph-details-clear';
    clearBtn.type = 'button';
    clearBtn.textContent = 'Limpar seleção';
    clearBtn.onclick = clearSelection;
    head.append(title, clearBtn);
    details.append(head);

    const badges = document.createElement('div');
    badges.className = 'graph-details-badges';
    const terms = commData?.terms || [];
    if (terms.length) {
      const termsLabel = document.createElement('span');
      termsLabel.textContent = 'Termos distintivos: ';
      badges.append(termsLabel);
      terms.forEach(term => {
        const span = document.createElement('span');
        span.className = 'graph-token';
        span.textContent = term;
        badges.append(span);
      });
    } else {
      badges.textContent = 'Sem termos distintivos preponderantes.';
    }
    details.append(badges);

    // Bridge nodes analysis
    const bridges = members.filter(m => Number(m.bridge_score) > 0);
    if (bridges.length) {
      const bridgeTitle = document.createElement('p');
      bridgeTitle.innerHTML = `<strong>Pontes com outras comunidades (${bridges.length}):</strong> Estas referências conectam C${key} a outros grupos temáticos.`;
      details.append(bridgeTitle);
    }

    const docList = document.createElement('ul');
    docList.className = 'graph-evidence-list';
    members.forEach(member => {
      const item = document.createElement('li');
      item.className = 'graph-evidence-item';
      const itemHead = document.createElement('div');
      itemHead.className = 'graph-evidence-item-head';
      const docTitle = document.createElement('span');
      docTitle.textContent = member.title || member.id;
      const inspectBtn = document.createElement('button');
      inspectBtn.className = 'graph-chip';
      inspectBtn.type = 'button';
      inspectBtn.textContent = 'Inspecionar nó';
      inspectBtn.onclick = () => selectNode(String(member.id));
      itemHead.append(docTitle, inspectBtn);
      item.append(itemHead);

      const itemMeta = document.createElement('div');
      itemMeta.className = 'graph-details-badges';
      itemMeta.innerHTML = `<span class="graph-badge">Grau ${member.degree}</span>` +
        (Number(member.bridge_score) > 0 ? `<span class="graph-badge bridge">Ponte (${Number(member.bridge_score).toFixed(2)})</span>` : '');
      const uri = safeRedapeUrl(member.uri);
      if (uri) {
        const link = document.createElement('a');
        link.href = uri;
        link.target = '_blank';
        link.rel = 'noopener noreferrer';
        link.textContent = 'Abrir referência original ↗';
        link.style.display = 'inline-block';
        link.style.marginTop = '0.3rem';
        itemMeta.append(link);
      }
      item.append(itemMeta);
      docList.append(item);
    });
    details.append(docList);
  }

  function selectNode(id) {
    clearSelection();
    const node = nodeById.get(id);
    if (!node) return;

    const connectedEdges = relationships.filter(e => String(e.source) === id || String(e.target) === id);
    const neighborIds = new Set();
    connectedEdges.forEach(e => {
      neighborIds.add(String(e.source) === id ? String(e.target) : String(e.source));
    });

    nodeElements.get(id)?.classList.add('is-selected');
    neighborIds.forEach(nId => nodeElements.get(nId)?.classList.add('is-neighbor'));
    nodeElements.forEach((el, nId) => {
      if (nId !== id && !neighborIds.has(nId)) el.classList.add('is-dimmed');
    });

    edgeElements.forEach(({ element, sId, tId }) => {
      if (sId === id || tId === id) element.classList.add('is-active-edge');
      else element.classList.add('is-hidden-edge');
    });

    details.replaceChildren();
    const head = document.createElement('div');
    head.className = 'graph-details-header';
    const title = document.createElement('strong');
    title.textContent = node.title || id;
    const clearBtn = document.createElement('button');
    clearBtn.className = 'graph-details-clear';
    clearBtn.type = 'button';
    clearBtn.textContent = 'Limpar seleção';
    clearBtn.onclick = clearSelection;
    head.append(title, clearBtn);
    details.append(head);

    const badges = document.createElement('div');
    badges.className = 'graph-details-badges';
    const commTag = document.createElement('button');
    commTag.className = 'graph-badge community-tag';
    commTag.type = 'button';
    commTag.textContent = `Comunidade C${node.community} ↗`;
    commTag.title = 'Clique para filtrar por esta comunidade';
    commTag.onclick = () => selectCommunity(node.community);
    badges.append(commTag);

    const degSpan = document.createElement('span');
    degSpan.className = 'graph-badge';
    degSpan.textContent = `Grau ${node.degree}`;
    badges.append(degSpan);

    if (Number(node.bridge_score) > 0) {
      const bridgeSpan = document.createElement('span');
      bridgeSpan.className = 'graph-badge bridge';
      bridgeSpan.textContent = `Bridge Score ${Number(node.bridge_score).toFixed(2)} (Ponte)`;
      badges.append(bridgeSpan);
    }
    if (Number(node.novelty_score) !== undefined) {
      const novSpan = document.createElement('span');
      novSpan.className = 'graph-badge';
      novSpan.textContent = `Novidade ${Number(node.novelty_score).toFixed(2)}`;
      badges.append(novSpan);
    }
    details.append(badges);

    const uri = safeRedapeUrl(node.uri);
    if (uri) {
      const link = document.createElement('a');
      link.href = uri;
      link.target = '_blank';
      link.rel = 'noopener noreferrer';
      link.textContent = 'Abrir referência original ↗';
      details.append(link);
    }

    const subtitle = document.createElement('p');
    subtitle.innerHTML = `<strong>Relações lexicais identificadas (${connectedEdges.length}):</strong> Ligações candidatas calculadas; termos compartilhados justificam a similaridade.`;
    details.append(subtitle);

    if (!connectedEdges.length) {
      const emptyMsg = document.createElement('p');
      emptyMsg.textContent = 'Nenhuma ligação lexical encontrada no grafo para esta referência.';
      details.append(emptyMsg);
      return;
    }

    connectedEdges.sort((a, b) => Number(b.weight) - Number(a.weight));
    const list = document.createElement('ul');
    list.className = 'graph-evidence-list';

    for (const edge of connectedEdges) {
      const otherId = String(edge.source) === id ? String(edge.target) : String(edge.source);
      const other = nodeById.get(otherId);
      const item = document.createElement('li');
      item.className = 'graph-evidence-item';

      const itemHead = document.createElement('div');
      itemHead.className = 'graph-evidence-item-head';
      const otherTitle = document.createElement('span');
      otherTitle.textContent = other?.title || otherId;

      const inspectBtn = document.createElement('button');
      inspectBtn.className = 'graph-chip';
      inspectBtn.type = 'button';
      inspectBtn.textContent = 'Inspecionar vizinho';
      inspectBtn.onclick = () => selectNode(otherId);
      itemHead.append(otherTitle, inspectBtn);
      item.append(itemHead);

      const metaRow = document.createElement('div');
      metaRow.className = 'graph-details-badges';
      const simBadge = document.createElement('span');
      simBadge.className = 'graph-badge';
      simBadge.textContent = `Similaridade ${Number(edge.weight).toFixed(2)}`;
      metaRow.append(simBadge);

      if (other && String(other.community) !== String(node.community)) {
        const bridgeBadge = document.createElement('span');
        bridgeBadge.className = 'graph-badge bridge';
        bridgeBadge.textContent = `Ponte com C${other.community}`;
        metaRow.append(bridgeBadge);
      }
      item.append(metaRow);

      const evidence = Array.isArray(edge.evidence) ? edge.evidence : [];
      if (evidence.length) {
        const tokensRow = document.createElement('div');
        tokensRow.className = 'graph-evidence-tokens';
        const label = document.createElement('span');
        label.style.fontSize = '0.75rem';
        label.textContent = 'Evidência: ';
        tokensRow.append(label);
        evidence.forEach(t => {
          const tok = document.createElement('span');
          tok.className = 'graph-token';
          tok.textContent = t;
          tokensRow.append(tok);
        });
        item.append(tokensRow);
      }
      list.append(item);
    }
    details.append(list);
  }

  function selectEdge(edge) {
    clearSelection();
    const sId = String(edge.source);
    const tId = String(edge.target);
    const sNode = nodeById.get(sId);
    const tNode = nodeById.get(tId);

    nodeElements.get(sId)?.classList.add('is-selected');
    nodeElements.get(tId)?.classList.add('is-selected');
    nodeElements.forEach((el, id) => {
      if (id !== sId && id !== tId) el.classList.add('is-dimmed');
    });

    edgeElements.forEach(({ element, edge: cand }) => {
      if (cand === edge) element.classList.add('is-active-edge');
      else element.classList.add('is-hidden-edge');
    });

    details.replaceChildren();
    const head = document.createElement('div');
    head.className = 'graph-details-header';
    const title = document.createElement('strong');
    title.textContent = `Conexão Lexical Calculada · Similaridade ${Number(edge.weight).toFixed(3)}`;
    const clearBtn = document.createElement('button');
    clearBtn.className = 'graph-details-clear';
    clearBtn.type = 'button';
    clearBtn.textContent = 'Limpar seleção';
    clearBtn.onclick = clearSelection;
    head.append(title, clearBtn);
    details.append(head);

    const isBridge = sNode && tNode && String(sNode.community) !== String(tNode.community);
    if (isBridge) {
      const bridgeNotice = document.createElement('p');
      bridgeNotice.className = 'graph-badge bridge';
      bridgeNotice.textContent = `Ponte entre Comunidade C${sNode.community} e Comunidade C${tNode.community}`;
      details.append(bridgeNotice);
    }

    const pairList = document.createElement('ul');
    pairList.className = 'graph-evidence-list';

    [sNode, tNode].forEach((itemNode) => {
      if (!itemNode) return;
      const li = document.createElement('li');
      li.className = 'graph-evidence-item';
      const iHead = document.createElement('div');
      iHead.className = 'graph-evidence-item-head';
      const t = document.createElement('span');
      t.textContent = itemNode.title || itemNode.id;
      const btn = document.createElement('button');
      btn.className = 'graph-chip';
      btn.type = 'button';
      btn.textContent = 'Inspecionar nó';
      btn.onclick = () => selectNode(String(itemNode.id));
      iHead.append(t, btn);
      li.append(iHead);

      const b = document.createElement('div');
      b.className = 'graph-details-badges';
      b.innerHTML = `<span class="graph-badge">Comunidade C${itemNode.community}</span> <span class="graph-badge">Grau ${itemNode.degree}</span>`;
      li.append(b);
      pairList.append(li);
    });
    details.append(pairList);

    const evidence = Array.isArray(edge.evidence) ? edge.evidence : [];
    if (evidence.length) {
      const tokContainer = document.createElement('div');
      tokContainer.innerHTML = '<strong>Termos que sustentam esta aproximação:</strong>';
      const tokensRow = document.createElement('div');
      tokensRow.className = 'graph-evidence-tokens';
      evidence.forEach(t => {
        const tok = document.createElement('span');
        tok.className = 'graph-token';
        tok.textContent = t;
        tokensRow.append(tok);
      });
      tokContainer.append(tokensRow);
      details.append(tokContainer);
    }
  }

  graphSelectionHandler = { selectCommunity, selectNode, selectEdge, clearSelection };
  clearSelection();

  let zoom = 1;
  const applyZoom = factor => {
    zoom = Math.max(0.8, Math.min(3, zoom * factor));
    const visibleWidth = width / zoom;
    const visibleHeight = height / zoom;
    svg.setAttribute('viewBox', `${(width - visibleWidth) / 2} ${(height - visibleHeight) / 2} ${visibleWidth} ${visibleHeight}`);
  };
  $('#graph-zoom-in').onclick = () => applyZoom(1.25);
  $('#graph-zoom-out').onclick = () => applyZoom(0.8);
  $('#graph-zoom-reset').onclick = () => { zoom = 1; applyZoom(1); };
}

$('#run-graph-analysis').addEventListener('click', async (event) => {
  const button = event.currentTarget;
  const status = $('#graph-status');
  button.disabled = true;
  status.textContent = 'Aprendendo estrutura do grafo…';

  try {
    const scopeType = $('#graph-scope-select')?.value || 'catalog';
    let body = '{}';

    if (scopeType === 'search_active') {
      if (lastSearchResults.length < 2) {
        throw new Error('A pesquisa ativa possui menos de 2 referências para calcular relações.');
      }
      body = JSON.stringify({
        documents: lastSearchResults.map(r => ({
          id: r.external_id || r.id,
          title: r.title,
          uri: r.canonical_uri || r.uri
        }))
      });
    } else if (scopeType === 'filter') {
      const query = $('#graph-scope-query')?.value.trim() || '';
      if (!query) {
        throw new Error('Informe um termo no campo de busca para filtrar o catálogo.');
      }
      body = JSON.stringify({ query });
    }

    const result = await authApi('/api/v1/analysis/graph', { method: 'POST', body });
    const scopeNotice = result.scope ? ` [escopo: ${result.scope}]` : '';
    status.textContent = `${result.input_count} referências${scopeNotice} · ${result.edge_count} relações · ${result.communities.length} comunidades. Padrões candidatos.`;
    renderGraphNetwork(result);

    $('#community-grid').replaceChildren(...result.communities.map((community) => {
      const card = document.createElement('article');
      card.tabIndex = 0;
      card.role = 'button';
      card.setAttribute('aria-label', `Selecionar comunidade C${String(community.id).padStart(2, '0')}`);
      const title = document.createElement('strong');
      title.textContent = `C${String(community.id).padStart(2, '0')} · ${community.size} referência(s)`;
      const terms = document.createElement('p');
      terms.textContent = community.terms.join(' · ') || 'sem termos distintivos';
      card.append(title, terms);
      card.onclick = () => {
        if (graphSelectionHandler) graphSelectionHandler.selectCommunity(community.id);
      };
      card.onkeydown = (e) => {
        if (e.key === 'Enter' || e.key === ' ') {
          e.preventDefault();
          if (graphSelectionHandler) graphSelectionHandler.selectCommunity(community.id);
        }
      };
      return card;
    }));
  } catch (failure) {
    status.textContent = failure.message;
    $('#graph-map').hidden = true;
    $('#community-grid').replaceChildren();
  } finally {
    button.disabled = false;
  }
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
