<?php

require_once __DIR__ . '/../vendor/autoload.php';

use Symfony\Component\HttpFoundation\Request;
use Symfony\Component\HttpFoundation\Response;
use Symfony\Component\HttpFoundation\JsonResponse;

// ===================== Configuration =====================
define('BUILDER_URL', 'http://127.0.0.1:9001');
define('COORDINATOR_URL', 'http://127.0.0.1:9002');
define('SESSION_FILE', '/tmp/buildgate_sessions.json');

// ===================== Session Management =====================
session_save_path('/tmp');
session_start();

function loadSessions()
{
    if (file_exists(SESSION_FILE)) {
        return json_decode(file_get_contents(SESSION_FILE), true) ?: [];
    }
    return [];
}

function saveSessions($sessions)
{
    file_put_contents(SESSION_FILE, json_encode($sessions));
}

function getUser()
{
    if (isset($_SESSION['user_id'])) {
        $sessions = loadSessions();
        $uid = $_SESSION['user_id'];
        if (isset($sessions[$uid])) {
            return $sessions[$uid];
        }
    }
    return null;
}

function loginUser($username, $password)
{
    $sessions = loadSessions();
    $users = [
        'developer' => [
            'username' => 'developer',
            'password' => 'Sp1r1tG4m3_2026_D3v3l0p3r',
            'role' => 'user',
            'id' => 'u_dev'
        ]
    ];

    $adminPassword = getenv('ADMIN_PASSWORD');
    if ($adminPassword !== false && $adminPassword !== '') {
        $users['admin'] = [
            'username' => 'admin',
            'password' => $adminPassword,
            'role' => 'admin',
            'id' => 'u_admin'
        ];
    }

    if (isset($users[$username]) && $users[$username]['password'] === $password) {
        $user = $users[$username];
        $sessions[$user['id']] = $user;
        saveSessions($sessions);
        $_SESSION['user_id'] = $user['id'];
        return $user;
    }
    return null;
}

function isAdmin()
{
    $user = getUser();
    return $user && $user['role'] === 'admin';
}

function logoutUser()
{
    if (isset($_SESSION['user_id'])) {
        $sessions = loadSessions();
        $uid = $_SESSION['user_id'];
        unset($sessions[$uid]);
        saveSessions($sessions);
    }

    $_SESSION = [];
    if (session_status() === PHP_SESSION_ACTIVE) {
        session_destroy();
    }
}

function sendJson($data, $status = 200)
{
    (new JsonResponse($data, $status))->send();
    exit;
}

function requireUser()
{
    $user = getUser();
    if (!$user) {
        sendJson(['error' => 'Not authenticated'], 401);
    }
    return $user;
}

function normalizeRoutePath($pathInfo)
{
    return $pathInfo === '' || $pathInfo === '/' ? '/' : '/' . ltrim($pathInfo, '/');
}

function deriveGatewayPath(Request $request)
{
    $pathInfo = $request->getPathInfo();
    $requestPath = parse_url($_SERVER['REQUEST_URI'] ?? '', PHP_URL_PATH) ?: '';

    if (empty($_SERVER['PATH_INFO']) && preg_match('#^/index\.php/(admin|internal)(?:/.*)?$#', $requestPath)) {
        return ltrim(substr($requestPath, strlen('/index.php/')), '/');
    }

    return $pathInfo;
}

function userCanAccessProject($user, $project)
{
    return $user['role'] === 'admin' || $project['owner'] === $user['id'];
}

function userCanAccessBuild($user, $build)
{
    return $user['role'] === 'admin' || $build['user'] === $user['id'];
}

function nowStamp()
{
    return date('Y-m-d H:i:s');
}

function platformCatalog()
{
    return [
        'templates' => [
            [
                'id' => 'composer-preview',
                'label' => '依赖预览',
                'notify_channel' => '#preview-desk',
                'retention_days' => 7,
                'target_php' => '8.2'
            ],
            [
                'id' => 'partner-audit',
                'label' => '伙伴接入',
                'notify_channel' => '#partner-audit',
                'retention_days' => 14,
                'target_php' => '8.2'
            ],
            [
                'id' => 'nightly-validation',
                'label' => '夜间回归',
                'notify_channel' => '#nightly-preview',
                'retention_days' => 5,
                'target_php' => '8.1'
            ],
        ],
        'teams' => ['runtime', 'application', 'integration', 'release'],
        'change_windows' => ['business-hours', 'nightly', 'manual'],
        'retention_choices' => [3, 5, 7, 14, 30],
    ];
}

function platformNotices()
{
    return [
        [
            'id' => 'ntc_001',
            'title' => '周末窗口维护',
            'body' => '本周六 21:00 后将轮转临时构建缓存，历史预览项目保留策略不受影响。'
        ],
        [
            'id' => 'ntc_002',
            'title' => '依赖清单校验',
            'body' => '从本周开始，新建项目默认启用基础依赖清单归档，用于审计留痕。'
        ],
    ];
}

function createProjectRecord($projectId, $payload, $user)
{
    $catalog = platformCatalog();
    $templateId = $payload['template'] ?? 'composer-preview';
    $template = $catalog['templates'][0];
    foreach ($catalog['templates'] as $candidate) {
        if ($candidate['id'] === $templateId) {
            $template = $candidate;
            break;
        }
    }

    $retentionDays = (int) ($payload['retention_days'] ?? $template['retention_days']);
    if ($retentionDays <= 0) {
        $retentionDays = $template['retention_days'];
    }

    $rawTags = $payload['tags'] ?? 'preview,adhoc';
    $tags = array_values(array_filter(array_map('trim', explode(',', $rawTags))));
    if (empty($tags)) {
        $tags = ['preview'];
    }

    return [
        'id' => $projectId,
        'name' => $payload['name'] ?? 'Untitled',
        'owner' => $user['id'],
        'created' => nowStamp(),
        'namespace' => $payload['namespace'] ?? 'sandbox',
        'team' => $payload['team'] ?? 'runtime',
        'template' => $templateId,
        'notify_channel' => $payload['notify_channel'] ?? $template['notify_channel'],
        'retention_days' => $retentionDays,
        'target_php' => $payload['target_php'] ?? $template['target_php'],
        'change_window' => $payload['change_window'] ?? 'business-hours',
        'visibility' => 'private',
        'workspace_policy' => 'archive',
        'transport' => 'composer',
        'repositories' => [],
        'builds' => [],
        'notes' => [],
        'activity' => [],
        'tags' => $tags,
        'compliance' => [
            'sbom' => 'enabled',
            'owner_review' => 'required',
            'policy' => 'preview-only'
        ],
        'routing' => [
            'lane' => 'preview',
            'priority' => 'normal'
        ],
    ];
}

function appendProjectActivity(&$allData, $projectId, $actor, $category, $message, $meta = [])
{
    if (!isset($allData['projects'][$projectId])) {
        return;
    }

    if (!isset($allData['projects'][$projectId]['activity'])) {
        $allData['projects'][$projectId]['activity'] = [];
    }

    $allData['projects'][$projectId]['activity'][] = [
        'id' => 'evt_' . bin2hex(random_bytes(4)),
        'time' => nowStamp(),
        'actor' => $actor['username'] ?? 'system',
        'category' => $category,
        'message' => $message,
        'meta' => $meta,
    ];

    if (count($allData['projects'][$projectId]['activity']) > 40) {
        $allData['projects'][$projectId]['activity'] = array_slice($allData['projects'][$projectId]['activity'], -40);
    }
}

function summarizeProject($project)
{
    $lastBuildId = null;
    if (!empty($project['builds'])) {
        $lastBuildId = $project['builds'][count($project['builds']) - 1];
    }

    return [
        'id' => $project['id'],
        'name' => $project['name'],
        'namespace' => $project['namespace'] ?? 'sandbox',
        'team' => $project['team'] ?? 'runtime',
        'template' => $project['template'] ?? 'composer-preview',
        'notify_channel' => $project['notify_channel'] ?? '#preview-desk',
        'retention_days' => $project['retention_days'] ?? 7,
        'target_php' => $project['target_php'] ?? '8.2',
        'change_window' => $project['change_window'] ?? 'business-hours',
        'transport' => $project['transport'] ?? 'composer',
        'workspace_policy' => $project['workspace_policy'] ?? 'archive',
        'tags' => $project['tags'] ?? [],
        'last_build' => $lastBuildId,
        'build_count' => count($project['builds'] ?? []),
    ];
}

function visibleProjects($user, $allData)
{
    $items = [];
    foreach ($allData['projects'] ?? [] as $project) {
        if (userCanAccessProject($user, $project)) {
            $items[] = summarizeProject($project);
        }
    }

    usort($items, function ($a, $b) {
        return strcmp($b['id'], $a['id']);
    });

    return array_slice($items, 0, 12);
}

function visibleBuilds($user, $allData, $limit = 10)
{
    $items = [];
    foreach ($allData['builds'] ?? [] as $build) {
        if (userCanAccessBuild($user, $build)) {
            $items[] = [
                'id' => $build['id'],
                'project' => $build['project'],
                'requested_by' => $build['requested_by'] ?? $build['user'],
                'status' => $build['result']['success'] ?? false ? 'completed' : 'failed',
                'transport' => $build['transport_label'] ?? ($build['driver'] ?? 'composer'),
                'mode' => $build['mode_label'] ?? ($build['profile'] ?? 'default'),
                'timestamp' => $build['timestamp'],
                'duration_ms' => $build['duration_ms'] ?? null,
            ];
        }
    }

    usort($items, function ($a, $b) {
        return strcmp($b['id'], $a['id']);
    });

    return array_slice($items, 0, $limit);
}

function visibleActivity($user, $allData, $limit = 12)
{
    $items = [];
    foreach ($allData['projects'] ?? [] as $project) {
        if (!userCanAccessProject($user, $project)) {
            continue;
        }

        foreach ($project['activity'] ?? [] as $event) {
            $items[] = [
                'time' => $event['time'],
                'project' => $project['name'],
                'category' => $event['category'],
                'message' => $event['message'],
                'actor' => $event['actor'],
            ];
        }
    }

    usort($items, function ($a, $b) {
        return strcmp($b['time'], $a['time']);
    });

    return array_slice($items, 0, $limit);
}

function executionProfileForProject($project)
{
    if (!empty($project['execution_mode'])) {
        return $project['execution_mode'];
    }
    return ($project['workspace_policy'] ?? 'archive') === 'workspace' ? 'workspace' : 'default';
}

function transportForProject($project)
{
    return $project['transport'] ?? 'composer';
}

function normalizeIntegrationType($channel)
{
    return $channel === 'legacy-sync' ? 'perforce' : $channel;
}

function normalizeExecutionMode($mode)
{
    return $mode === 'workspace' ? 'workspace' : 'default';
}

function dashboardPayload($user, $allData)
{
    return [
        'user' => [
            'username' => $user['username'],
            'role' => $user['role']
        ],
        'projects' => visibleProjects($user, $allData),
        'recent_builds' => visibleBuilds($user, $allData),
        'recent_activity' => visibleActivity($user, $allData),
        'catalog' => platformCatalog(),
        'notices' => platformNotices(),
    ];
}

// ===================== Project & Build Storage =====================
define('DATA_FILE', '/tmp/buildgate_data.json');

function loadData()
{
    if (file_exists(DATA_FILE)) {
        return json_decode(file_get_contents(DATA_FILE), true) ?: [];
    }
    return ['projects' => [], 'builds' => [], 'repositories' => []];
}

function saveData($data)
{
    file_put_contents(DATA_FILE, json_encode($data));
}

// ===================== Request Handling =====================
$request = Request::createFromGlobals();
$method = $request->getMethod();
$pathInfo = deriveGatewayPath($request);
$routePath = normalizeRoutePath($pathInfo);
$isAdminPath = str_starts_with($pathInfo, '/admin') || str_starts_with($pathInfo, '/internal');
if ($isAdminPath) {
    if (!isAdmin()) {
        sendJson(['error' => 'Forbidden'], 403);
    }
}

if ($method === 'GET' && $routePath === '/') {
    if (getUser()) {
        $response = new Response('', 302, ['Location' => '/workspace']);
        $response->send();
        exit;
    }

    $html = <<<'HTML'
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>BuildGate - 登录</title>
    <style>
        :root {
            --bg: #edf2f4;
            --panel: #ffffff;
            --panel-soft: #f6f8fa;
            --text: #17232c;
            --muted: #5d6e7c;
            --border: #d7dfe5;
            --accent: #145c63;
            --accent-strong: #0f474d;
            --shadow: 0 24px 48px rgba(14, 24, 32, 0.08);
        }

        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            min-height: 100vh;
            display: grid;
            place-items: center;
            padding: 24px 16px;
            background:
                radial-gradient(circle at top left, rgba(20, 92, 99, 0.08), transparent 32%),
                linear-gradient(180deg, #f7fafb 0%, #edf2f4 100%);
            color: var(--text);
            font-family: "IBM Plex Sans", "Noto Sans SC", "PingFang SC", "Microsoft YaHei", sans-serif;
        }

        .shell {
            width: min(1120px, 100%);
            display: grid;
            grid-template-columns: minmax(0, 1.1fr) minmax(360px, 0.78fr);
            gap: 18px;
        }

        .hero,
        .login-card {
            border: 1px solid var(--border);
            border-radius: 10px;
            background: var(--panel);
            box-shadow: var(--shadow);
        }

        .hero {
            padding: 34px 36px;
            display: grid;
            gap: 28px;
        }

        .brand {
            color: var(--accent);
            font-size: 12px;
            font-weight: 700;
            letter-spacing: 0.08em;
            text-transform: uppercase;
        }

        h1 {
            font-size: clamp(36px, 4vw, 50px);
            line-height: 1.03;
            color: #0d1922;
            max-width: 620px;
        }

        .lead {
            max-width: 680px;
            color: var(--muted);
            font-size: 15px;
            line-height: 1.8;
        }

        .hero-grid {
            display: grid;
            grid-template-columns: repeat(3, minmax(0, 1fr));
            gap: 12px;
        }

        .stat {
            border: 1px solid var(--border);
            border-radius: 8px;
            background: var(--panel-soft);
            padding: 16px 14px;
        }

        .stat b {
            display: block;
            font-size: 11px;
            color: var(--muted);
            text-transform: uppercase;
            letter-spacing: 0.05em;
            margin-bottom: 8px;
        }

        .stat span {
            font-size: 18px;
            font-weight: 700;
            color: var(--accent-strong);
        }

        .hero-band {
            display: grid;
            grid-template-columns: repeat(2, minmax(0, 1fr));
            gap: 12px;
        }

        .hero-card {
            border: 1px solid var(--border);
            border-radius: 8px;
            background: linear-gradient(180deg, #fcfefe 0%, #f4f8f9 100%);
            padding: 18px;
        }

        .hero-card b {
            display: block;
            color: #0d1922;
            font-size: 13px;
            margin-bottom: 8px;
        }

        .hero-card span {
            color: var(--muted);
            font-size: 13px;
            line-height: 1.75;
        }

        .login-card {
            padding: 30px 28px;
            display: grid;
            align-content: center;
            gap: 20px;
        }

        .login-card h2 {
            font-size: 26px;
            color: #0d1922;
        }

        .login-card p {
            color: var(--muted);
            font-size: 14px;
            line-height: 1.75;
        }

        .form {
            display: grid;
            gap: 16px;
        }

        label {
            display: block;
            font-size: 12px;
            font-weight: 700;
            color: #0f1d26;
            margin-bottom: 8px;
        }

        input,
        button {
            width: 100%;
            border-radius: 8px;
            font-size: 14px;
        }

        input {
            padding: 12px 14px;
            border: 1px solid var(--border);
            background: #fcfdfe;
            color: var(--text);
            outline: none;
        }

        input:focus {
            border-color: rgba(20, 92, 99, 0.45);
            box-shadow: 0 0 0 4px rgba(20, 92, 99, 0.08);
        }

        button {
            padding: 12px 14px;
            border: none;
            background: var(--accent);
            color: #fff;
            font-weight: 700;
            cursor: pointer;
            transition: background 120ms ease, transform 120ms ease;
        }

        button:hover {
            background: var(--accent-strong);
            transform: translateY(-1px);
        }

        .result-box {
            min-height: 96px;
            padding: 14px;
            border-radius: 8px;
            border: 1px solid var(--border);
            background: #0f1720;
            color: #dbe6f0;
            font-family: "IBM Plex Mono", "Consolas", monospace;
            font-size: 12px;
            line-height: 1.65;
            white-space: pre-wrap;
            overflow: auto;
        }

        .note {
            border: 1px solid var(--border);
            border-radius: 8px;
            background: var(--panel-soft);
            padding: 14px;
            color: var(--muted);
            font-size: 13px;
            line-height: 1.7;
        }

        @media (max-width: 960px) {
            .shell,
            .hero-grid,
            .hero-band {
                grid-template-columns: 1fr;
            }
        }
    </style>
</head>
<body>
    <div class="shell">
        <section class="hero">
            <div class="brand">BuildGate</div>
            <div>
                <h1>内部依赖预览平台</h1>
                <p class="lead">用于创建临时项目、执行依赖预览构建、记录通知策略与保留规则。</p>
            </div>
            <div class="hero-grid">
                <div class="stat"><b>Web</b><span>Online</span></div>
                <div class="stat"><b>Builder</b><span>Ready</span></div>
                <div class="stat"><b>Coordinator</b><span>Internal</span></div>
            </div>
            <div class="hero-band">
                <div class="hero-card">
                    <b>工作区生命周期</b>
                    <span>项目模板、通知策略和保留规则会在工作台中持续生效，用于支持短周期依赖验证与构建回溯。</span>
                </div>
                <div class="hero-card">
                    <b>协作归档</b>
                    <span>构建摘要、项目动态和操作记录会统一归档，便于团队按项目回溯最近的预览过程。</span>
                </div>
            </div>
        </section>
        <section class="login-card">
            <div>
                <div class="brand">Access Portal</div>
                <h2>登录到工作台</h2>
                <p>使用内部账号进入工作台，管理项目、查看最近构建，并继续后续的预览流程。</p>
            </div>
            <div class="form">
                <div>
                    <label for="username">用户名</label>
                    <input type="text" id="username" placeholder="请输入用户名">
                </div>
                <div>
                    <label for="password">密码</label>
                    <input type="password" id="password" placeholder="请输入密码">
                </div>
                <button onclick="login()">进入工作台</button>
            </div>
            <div class="note">当前入口只处理登录。项目、构建、公告与审计视图会在登录成功后以独立工作台页面加载。</div>
            <pre id="loginResult" class="result-box">等待登录…</pre>
        </section>
    </div>
    <script>
        function renderResult(id, data) {
            document.getElementById(id).innerText = typeof data === 'string' ? data : JSON.stringify(data, null, 2);
        }

        function setHtml(id, html) {
            const node = document.getElementById(id);
            if (node) {
                node.innerHTML = html;
            }
        }

        function setNodeText(id, text) {
            const node = document.getElementById(id);
            if (node) {
                node.innerText = text;
            }
        }

        async function login() {
            const username = document.getElementById('username').value;
            const password = document.getElementById('password').value;
            const resp = await fetch('/login', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({username, password})
            });
            const data = await resp.json();
            renderResult('loginResult', data);
            if (data.success) {
                window.location.href = '/workspace';
            }
        }
    </script>
</body>
</html>
HTML;
    $response = new Response($html, 200, ['Content-Type' => 'text/html']);
    $response->send();
    exit;
}

if ($method === 'GET' && $routePath === '/workspace') {
    if (!getUser()) {
        $response = new Response('', 302, ['Location' => '/']);
        $response->send();
        exit;
    }

    // Landing page
    $html = <<<'HTML'
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>BuildGate - 内部依赖预览平台</title>
    <style>
        :root {
            --bg: #eef2f5;
            --panel: #ffffff;
            --panel-alt: #f6f8fa;
            --text: #16222b;
            --muted: #62717f;
            --border: #d9e0e6;
            --accent: #1a5f67;
            --accent-strong: #12484e;
            --ink: #0e1b24;
            --shadow: 0 12px 30px rgba(16, 24, 32, 0.06);
        }

        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            min-height: 100vh;
            background: linear-gradient(180deg, #f5f7f9 0%, #edf2f5 100%);
            color: var(--text);
            font-family: "IBM Plex Sans", "Noto Sans SC", "PingFang SC", "Microsoft YaHei", sans-serif;
        }

        .shell {
            width: min(1120px, calc(100vw - 32px));
            margin: 28px auto 40px;
        }

        .topbar,
        .panel,
        .side-panel {
            border: 1px solid var(--border);
            border-radius: 8px;
            background: var(--panel);
            box-shadow: var(--shadow);
        }

        .topbar {
            padding: 26px 28px;
            margin-bottom: 18px;
            display: flex;
            align-items: flex-start;
            justify-content: space-between;
            gap: 18px;
        }

        .brand {
            color: var(--accent);
            font-size: 12px;
            font-weight: 700;
            margin-bottom: 10px;
        }

        h1 {
            font-size: clamp(28px, 3vw, 38px);
            line-height: 1.1;
            color: var(--ink);
            margin-bottom: 12px;
        }

        .topbar p {
            max-width: 720px;
            color: var(--muted);
            line-height: 1.7;
            font-size: 14px;
        }

        .topbar-status {
            min-width: 280px;
            display: grid;
            gap: 10px;
        }

        .topbar-actions {
            display: grid;
            gap: 10px;
            min-width: 300px;
        }

        .identity-card {
            border: 1px solid var(--border);
            border-radius: 8px;
            background: var(--panel-alt);
            padding: 12px 14px;
        }

        .identity-card b {
            display: block;
            font-size: 12px;
            color: var(--ink);
            margin-bottom: 6px;
        }

        .identity-card span {
            display: block;
            color: var(--muted);
            font-size: 13px;
            line-height: 1.6;
        }

        .status-row {
            border: 1px solid var(--border);
            border-radius: 8px;
            background: var(--panel-alt);
            padding: 12px 14px;
        }

        .status-row b {
            display: block;
            font-size: 12px;
            color: var(--ink);
            margin-bottom: 6px;
        }

        .status-row span {
            display: block;
            font-size: 13px;
            color: var(--muted);
        }

        .panel h2,
        .side-panel h2 {
            font-size: 16px;
            line-height: 1.3;
            color: var(--ink);
            margin-bottom: 10px;
        }

        .topbar-meta {
            display: flex;
            flex-wrap: wrap;
            gap: 10px;
            margin-top: 16px;
        }

        .meta-pill {
            display: inline-flex;
            align-items: center;
            border-radius: 999px;
            padding: 7px 11px;
            font-size: 12px;
            font-weight: 700;
            border: 1px solid var(--border);
            background: var(--panel-alt);
            color: var(--ink);
        }

        .workspace {
            display: grid;
            grid-template-columns: minmax(0, 1.85fr) minmax(260px, 0.92fr);
            gap: 18px;
        }

        .stack {
            display: grid;
            gap: 18px;
        }

        .panel,
        .side-panel {
            padding: 22px;
        }

        .panel-head {
            display: flex;
            align-items: flex-start;
            justify-content: space-between;
            gap: 16px;
            margin-bottom: 16px;
        }

        .panel-head p,
        .side-panel p,
        .meta-list li,
        .session-grid span,
        .bullet-list li {
            color: var(--muted);
            font-size: 13px;
            line-height: 1.65;
        }

        .panel-tag {
            flex: 0 0 auto;
            display: inline-flex;
            align-items: center;
            border-radius: 999px;
            border: 1px solid var(--border);
            padding: 6px 10px;
            font-size: 12px;
            font-weight: 700;
            color: var(--accent);
            background: #eef5f6;
        }

        .panel-grid {
            display: grid;
            grid-template-columns: repeat(2, minmax(0, 1fr));
            gap: 14px;
        }

        label {
            display: block;
            color: var(--ink);
            font-size: 12px;
            font-weight: 700;
            margin-bottom: 8px;
        }

        input,
        button {
            width: 100%;
            border-radius: 8px;
            font-size: 14px;
        }

        input {
            padding: 12px 14px;
            border: 1px solid var(--border);
            background: #fcfdfe;
            color: var(--text);
            outline: none;
        }

        input:focus {
            border-color: rgba(15, 118, 110, 0.5);
            box-shadow: 0 0 0 4px rgba(15, 118, 110, 0.08);
        }

        button {
            padding: 12px 14px;
            border: none;
            background: var(--accent);
            color: #ffffff;
            font-weight: 700;
            cursor: pointer;
            transition: background 120ms ease, transform 120ms ease;
        }

        button:hover {
            background: var(--accent-strong);
            transform: translateY(-1px);
        }

        .secondary-button {
            background: #24475e;
        }

        .secondary-button:hover {
            background: #19384d;
        }

        .ghost-button {
            background: #f4f7f8;
            color: var(--ink);
            border: 1px solid var(--border);
        }

        .ghost-button:hover {
            background: #ebf1f3;
        }

        .result-box,
        .log-box {
            margin-top: 14px;
            border-radius: 8px;
            border: 1px solid var(--border);
            background: #0f1720;
            color: #dbe6f0;
            font-family: "IBM Plex Mono", "Consolas", monospace;
            font-size: 12px;
            line-height: 1.65;
            white-space: pre-wrap;
            overflow: auto;
        }

        .result-box {
            min-height: 96px;
            padding: 14px;
        }

        .log-box {
            max-height: 360px;
            padding: 16px;
            display: none;
        }

        .session-grid {
            display: grid;
            gap: 10px;
            margin-top: 8px;
        }

        .session-item {
            padding: 12px 14px;
            border-radius: 8px;
            background: var(--panel-alt);
            border: 1px solid var(--border);
        }

        .session-item b {
            display: block;
            font-size: 12px;
            color: var(--ink);
            margin-bottom: 5px;
        }

        .metric-strip {
            display: grid;
            grid-template-columns: repeat(3, minmax(0, 1fr));
            gap: 12px;
            margin-top: 16px;
        }

        .metric-tile {
            border: 1px solid var(--border);
            border-radius: 8px;
            background: var(--panel-alt);
            padding: 14px;
        }

        .metric-tile b {
            display: block;
            color: var(--muted);
            font-size: 11px;
            text-transform: uppercase;
            letter-spacing: 0.04em;
            margin-bottom: 8px;
        }

        .metric-tile span {
            display: block;
            color: var(--ink);
            font-size: 20px;
            font-weight: 700;
        }

        .compact-table {
            width: 100%;
            border-collapse: collapse;
            font-size: 12px;
            color: var(--text);
        }

        .compact-table th,
        .compact-table td {
            text-align: left;
            padding: 10px 0;
            border-bottom: 1px solid var(--border);
            vertical-align: top;
        }

        .compact-table th {
            font-size: 11px;
            font-weight: 700;
            color: var(--muted);
            text-transform: uppercase;
            letter-spacing: 0.04em;
        }

        .compact-table tr:last-child td {
            border-bottom: none;
        }

        .empty-state {
            color: var(--muted);
            font-size: 13px;
            line-height: 1.6;
        }

        .notice-list {
            display: grid;
            gap: 12px;
        }

        .notice-item {
            border: 1px solid var(--border);
            border-radius: 8px;
            background: var(--panel-alt);
            padding: 12px 14px;
        }

        .notice-item b {
            display: block;
            font-size: 12px;
            color: var(--ink);
            margin-bottom: 6px;
        }

        .meta-list,
        .bullet-list {
            list-style: none;
            display: grid;
            gap: 10px;
        }

        .bullet-list li::before,
        .meta-list li::before {
            content: "•";
            color: var(--accent);
            margin-right: 8px;
        }

        .role-badge {
            display: inline-flex;
            align-items: center;
            gap: 6px;
            padding: 6px 10px;
            border-radius: 999px;
            font-size: 12px;
            font-weight: 700;
            border: 1px solid var(--border);
        }

        .role-user {
            color: #0b7a48;
            background: #e8f8f0;
        }

        .role-admin {
            color: #8a2f0c;
            background: #fff1e8;
        }

        .topbar {
            display: grid;
            gap: 18px;
        }

        .topbar-main {
            display: flex;
            align-items: flex-start;
            justify-content: space-between;
            gap: 18px;
        }

        .topbar-copy {
            color: var(--muted);
            font-size: 13px;
            line-height: 1.65;
            margin-top: 6px;
        }

        .topbar-session {
            display: flex;
            align-items: center;
            gap: 10px;
            flex-wrap: wrap;
            justify-content: flex-end;
        }

        .session-badge {
            padding: 10px 12px;
            border-radius: 8px;
            border: 1px solid var(--border);
            background: var(--panel-alt);
            min-width: 180px;
        }

        .session-badge b {
            display: block;
            font-size: 11px;
            color: var(--muted);
            text-transform: uppercase;
            letter-spacing: 0.04em;
            margin-bottom: 6px;
        }

        .session-badge span {
            display: block;
            color: var(--ink);
            font-size: 13px;
            font-weight: 700;
        }

        .tab-bar {
            display: flex;
            gap: 10px;
            flex-wrap: wrap;
            border-top: 1px solid var(--border);
            padding-top: 16px;
        }

        .tab-pill {
            width: auto;
            min-width: 92px;
            padding: 10px 16px;
            border-radius: 999px;
            border: 1px solid var(--border);
            background: #f5f8f9;
            color: var(--muted);
            font-size: 13px;
            font-weight: 700;
            cursor: pointer;
            transition: background 120ms ease, color 120ms ease, border-color 120ms ease;
        }

        .tab-pill:hover {
            background: #ebf2f4;
            color: var(--ink);
            transform: none;
        }

        .tab-pill.active {
            background: var(--accent);
            color: #ffffff;
            border-color: var(--accent);
        }

        .page-view {
            display: none;
            gap: 18px;
        }

        .page-view.active {
            display: grid;
        }

        .grid-two {
            display: grid;
            grid-template-columns: repeat(2, minmax(0, 1fr));
            gap: 18px;
        }

        .grid-three {
            display: grid;
            grid-template-columns: repeat(3, minmax(0, 1fr));
            gap: 18px;
        }

        .hero-cards {
            display: grid;
            grid-template-columns: repeat(4, minmax(0, 1fr));
            gap: 12px;
        }

        .hero-card {
            border: 1px solid var(--border);
            border-radius: 8px;
            background: linear-gradient(180deg, #fcfefe 0%, #f4f8f9 100%);
            padding: 16px;
        }

        .hero-card b {
            display: block;
            color: var(--muted);
            font-size: 11px;
            text-transform: uppercase;
            letter-spacing: 0.04em;
            margin-bottom: 8px;
        }

        .hero-card span {
            display: block;
            color: var(--ink);
            font-size: 26px;
            font-weight: 700;
            margin-bottom: 6px;
        }

        .hero-card small {
            display: block;
            color: var(--muted);
            font-size: 12px;
            line-height: 1.55;
        }

        .toolbar {
            display: flex;
            align-items: center;
            justify-content: space-between;
            gap: 12px;
            margin-bottom: 14px;
        }

        .toolbar p {
            color: var(--muted);
            font-size: 13px;
            line-height: 1.6;
        }

        .inline-actions {
            display: flex;
            gap: 10px;
            flex-wrap: wrap;
        }

        .inline-actions button {
            width: auto;
            min-width: 140px;
        }

        .small-button {
            width: auto;
            min-width: 108px;
            padding: 9px 12px;
            font-size: 12px;
        }

        .detail-grid {
            display: grid;
            grid-template-columns: repeat(3, minmax(0, 1fr));
            gap: 12px;
        }

        .detail-card {
            border: 1px solid var(--border);
            border-radius: 8px;
            background: var(--panel-alt);
            padding: 14px;
        }

        .detail-card b {
            display: block;
            color: var(--muted);
            font-size: 11px;
            text-transform: uppercase;
            letter-spacing: 0.04em;
            margin-bottom: 8px;
        }

        .detail-card span {
            display: block;
            color: var(--ink);
            font-size: 14px;
            line-height: 1.6;
        }

        .tab-card-title {
            font-size: 18px;
            color: var(--ink);
            margin-bottom: 6px;
        }

        .tab-card-copy {
            color: var(--muted);
            font-size: 13px;
            line-height: 1.65;
            margin-bottom: 14px;
        }

        #tab-admin { display: none; }

        @media (max-width: 980px) {
            .topbar,
            .workspace,
            .panel-grid,
            .metric-strip,
            .grid-two,
            .grid-three,
            .hero-cards,
            .detail-grid,
            .topbar-main {
                grid-template-columns: 1fr;
                display: grid;
            }

            .topbar-session,
            .tab-bar,
            .toolbar,
            .inline-actions {
                justify-content: flex-start;
            }
        }

        .workspace {
            grid-template-columns: 1fr;
        }
    </style>
</head>
<body>
    <div class="shell">
        <section class="topbar">
            <div class="topbar-main">
                <div>
                    <div class="brand">BuildGate</div>
                    <h1>内部依赖预览平台</h1>
                    <div class="topbar-copy">项目、构建、运行记录和系统状态集中查看。</div>
                </div>
                <div class="topbar-session">
                    <div class="session-badge">
                        <b>当前用户</b>
                        <span id="userStatus">正在加载</span>
                    </div>
                    <div class="session-badge">
                        <b>工作台状态</b>
                        <span id="workspaceSession">已连接</span>
                    </div>
                    <button class="ghost-button" onclick="logout()">退出登录</button>
                </div>
            </div>
            <div class="tab-bar">
                <button id="tab-dashboard" class="tab-pill active" onclick="switchTab('dashboard')">看板</button>
                <button id="tab-projects" class="tab-pill" onclick="switchTab('projects')">项目</button>
                <button id="tab-builds" class="tab-pill" onclick="switchTab('builds')">构建</button>
                <button id="tab-records" class="tab-pill" onclick="switchTab('records')">记录</button>
                <button id="tab-admin" class="tab-pill" onclick="switchTab('admin')">管理</button>
            </div>
        </section>

        <main class="workspace">
            <section id="page-dashboard" class="page-view active">
                <div class="hero-cards">
                    <div class="hero-card">
                        <b>项目数</b>
                        <span id="metricProjects">0</span>
                        <small>当前账号可见项目</small>
                    </div>
                    <div class="hero-card">
                        <b>构建数</b>
                        <span id="metricBuilds">0</span>
                        <small>最近构建记录</small>
                    </div>
                    <div class="hero-card">
                        <b>动态数</b>
                        <span id="metricActivity">0</span>
                        <small>最近项目活动</small>
                    </div>
                    <div class="hero-card">
                        <b>当前项目</b>
                        <span id="metricCurrentProject">未选中</span>
                        <small>用于构建触发</small>
                    </div>
                </div>

                <div class="grid-two">
                    <section class="panel">
                        <div class="panel-head">
                            <div>
                                <h2>最近项目</h2>
                                <p>按创建时间展示当前账号可见的项目。</p>
                            </div>
                            <button class="ghost-button small-button" onclick="switchTab('projects')">打开项目</button>
                        </div>
                        <div id="dashboardProjectTable" class="empty-state">暂无记录。</div>
                    </section>
                    <section class="panel">
                        <div class="panel-head">
                            <div>
                                <h2>最近构建</h2>
                                <p>构建结果摘要和状态会在这里汇总。</p>
                            </div>
                            <button class="ghost-button small-button" onclick="switchTab('builds')">打开构建</button>
                        </div>
                        <div id="dashboardBuildTable" class="empty-state">暂无记录。</div>
                    </section>
                </div>

                <section class="panel">
                    <div class="panel-head">
                        <div>
                            <h2>运行简报</h2>
                            <p>平台维护、归档和运行状态相关消息。</p>
                        </div>
                    </div>
                    <div id="noticeList" class="notice-list">
                        <div class="notice-item">
                            <b>暂无简报</b>
                            <span class="empty-state">当前没有新的运行通知。</span>
                        </div>
                    </div>
                </section>
            </section>

            <section id="page-projects" class="page-view">
                <div class="grid-two">
                    <section class="panel">
                        <div class="panel-head">
                            <div>
                                <h2>新建项目</h2>
                                <p>创建一个临时项目用于依赖预览任务。</p>
                            </div>
                            <span class="panel-tag">项目</span>
                        </div>
                        <div class="panel-grid">
                            <div>
                                <label for="projectName">项目名称</label>
                                <input type="text" id="projectName" placeholder="请输入项目名称">
                            </div>
                            <div>
                                <label for="projectNamespace">业务命名空间</label>
                                <input type="text" id="projectNamespace" placeholder="例如 runtime/sandbox">
                            </div>
                            <div>
                                <label for="projectTemplate">项目模板</label>
                                <input type="text" id="projectTemplate" placeholder="例如 composer-preview">
                            </div>
                            <div>
                                <label for="projectTeam">归属团队</label>
                                <input type="text" id="projectTeam" placeholder="例如 runtime">
                            </div>
                            <div>
                                <label for="projectChannel">通知频道</label>
                                <input type="text" id="projectChannel" placeholder="例如 #preview-desk">
                            </div>
                            <div>
                                <label for="projectRetention">保留天数</label>
                                <input type="text" id="projectRetention" placeholder="例如 7">
                            </div>
                            <div>
                                <label for="projectWindow">变更窗口</label>
                                <input type="text" id="projectWindow" placeholder="例如 business-hours">
                            </div>
                            <div>
                                <label for="projectTags">标签</label>
                                <input type="text" id="projectTags" placeholder="例如 preview,adhoc">
                            </div>
                        </div>
                        <button onclick="createProject()">创建项目</button>
                        <pre id="projectResult" class="result-box">尚未创建项目。</pre>
                    </section>

                    <section class="panel">
                        <div class="panel-head">
                            <div>
                                <h2>项目列表</h2>
                                <p>项目基础信息、团队和最近构建记录。</p>
                            </div>
                        </div>
                        <div id="projectTable" class="empty-state">暂无记录。</div>
                    </section>
                </div>
            </section>

            <section id="page-builds" class="page-view">
                <div class="grid-two">
                    <section class="panel">
                        <div class="panel-head">
                            <div>
                                <h2>触发构建</h2>
                                <p>选择项目后提交一次预览构建。</p>
                            </div>
                            <span class="panel-tag">构建</span>
                        </div>
                        <label for="buildProject">构建目标 ID</label>
                        <input type="text" id="buildProject" placeholder="创建项目后自动填充">
                        <button class="secondary-button" onclick="triggerBuild()">执行 Preview Build</button>
                        <pre id="buildResult" class="result-box">等待任务触发。</pre>
                    </section>

                    <section class="panel">
                        <div class="panel-head">
                            <div>
                                <h2>最近构建</h2>
                                <p>按时间展示当前账号可见的构建记录。</p>
                            </div>
                        </div>
                        <div id="buildTable" class="empty-state">暂无记录。</div>
                    </section>
                </div>
                <section class="panel">
                    <div class="panel-head">
                        <div>
                            <h2>构建摘要</h2>
                            <p>最近一次构建返回的摘要输出。</p>
                        </div>
                    </div>
                    <div class="log-box" id="buildLog"></div>
                </section>
            </section>

            <section id="page-records" class="page-view">
                <div class="grid-two">
                    <section class="panel">
                        <div class="panel-head">
                            <div>
                                <h2>项目动态</h2>
                                <p>项目创建、策略更新、构建触发等操作记录。</p>
                            </div>
                        </div>
                        <div id="activityTable" class="empty-state">暂无记录。</div>
                    </section>
                    <section class="panel">
                        <div class="panel-head">
                            <div>
                                <h2>会话摘要</h2>
                                <p>当前登录角色和最近工作区状态。</p>
                            </div>
                        </div>
                        <div class="detail-grid">
                            <div class="detail-card">
                                <b>登录角色</b>
                                <span id="sessionRole">加载中</span>
                            </div>
                            <div class="detail-card">
                                <b>当前项目</b>
                                <span id="sessionProject">尚未创建</span>
                            </div>
                            <div class="detail-card">
                                <b>最近构建</b>
                                <span id="sessionBuild">尚未触发</span>
                            </div>
                        </div>
                    </section>
                </div>
            </section>

            <section id="page-admin" class="page-view">
                <section class="panel" id="adminSection">
                    <div class="panel-head">
                        <div>
                            <h2>管理</h2>
                            <p>当前会话可见的管理侧状态。</p>
                        </div>
                        <span class="role-badge role-admin">管理</span>
                    </div>
                    <pre id="adminResult" class="result-box">等待管理员数据。</pre>
                </section>
            </section>
        </main>
    </div>
    </div>

    <script>
        let currentUser = null;
        let currentProject = null;
        let currentBuild = null;
        let dashboardState = null;
        let currentTab = 'dashboard';

        function setText(id, text) {
            document.getElementById(id).innerText = text;
        }

        function renderResult(id, data) {
            document.getElementById(id).innerText = typeof data === 'string' ? data : JSON.stringify(data, null, 2);
        }

        function escapeHtml(value) {
            return String(value ?? '')
                .replaceAll('&', '&amp;')
                .replaceAll('<', '&lt;')
                .replaceAll('>', '&gt;')
                .replaceAll('"', '&quot;')
                .replaceAll("'", '&#39;');
        }

        function tableOrEmpty(columns, rows) {
            if (!rows || !rows.length) {
                return '<div class="empty-state">暂无记录。</div>';
            }
            const head = columns.map((item) => `<th>${item.label}</th>`).join('');
            const body = rows.map((row) => {
                const cells = columns.map((item) => `<td>${row[item.key] ?? ''}</td>`).join('');
                return `<tr>${cells}</tr>`;
            }).join('');
            return `<table class="compact-table"><thead><tr>${head}</tr></thead><tbody>${body}</tbody></table>`;
        }

        function renderNotices(notices) {
            const box = document.getElementById('noticeList');
            if (!notices || !notices.length) {
                box.innerHTML = '<div class="notice-item"><b>暂无公告</b><span class="empty-state">平台当前没有新的维护通知。</span></div>';
                return;
            }
            box.innerHTML = notices.map((item) => `
                <div class="notice-item">
                    <b>${escapeHtml(item.title)}</b>
                    <span>${escapeHtml(item.body)}</span>
                </div>
            `).join('');
        }

        function renderProjectTable(projects) {
            const rows = (projects || []).map((item) => ({
                name: `${escapeHtml(item.name)}<br><span style="color:#62717f">${escapeHtml(item.id)}</span>`,
                team: `${escapeHtml(item.team)}<br><span style="color:#62717f">${escapeHtml(item.namespace)}</span>`,
                retention: `${escapeHtml(item.retention_days)} 天`,
                last_build: escapeHtml(item.last_build || '未执行')
            }));
            const html = tableOrEmpty([
                {key: 'name', label: '项目'},
                {key: 'team', label: '归属'},
                {key: 'retention', label: '保留'},
                {key: 'last_build', label: '最近构建'}
            ], rows);
            setHtml('projectTable', html);
            setHtml('dashboardProjectTable', html);
        }

        function renderBuildTable(builds) {
            const rows = (builds || []).map((item) => ({
                id: escapeHtml(item.id),
                project: escapeHtml(item.project),
                mode: escapeHtml(item.mode),
                status: escapeHtml(item.status)
            }));
            const html = tableOrEmpty([
                {key: 'id', label: '构建 ID'},
                {key: 'project', label: '项目'},
                {key: 'mode', label: '模式'},
                {key: 'status', label: '状态'}
            ], rows);
            setHtml('buildTable', html);
            setHtml('dashboardBuildTable', html);
        }

        function renderActivityTable(items) {
            const rows = (items || []).map((item) => ({
                time: escapeHtml(item.time),
                actor: escapeHtml(item.actor),
                event: `${escapeHtml(item.message)}<br><span style="color:#62717f">${escapeHtml(item.project)} / ${escapeHtml(item.category)}</span>`
            }));
            document.getElementById('activityTable').innerHTML = tableOrEmpty([
                {key: 'time', label: '时间'},
                {key: 'actor', label: '操作人'},
                {key: 'event', label: '事件'}
            ], rows);
        }

        function switchTab(tab) {
            currentTab = tab;
            document.querySelectorAll('.tab-pill').forEach((node) => {
                node.classList.toggle('active', node.id === `tab-${tab}`);
            });
            document.querySelectorAll('.page-view').forEach((node) => {
                node.classList.toggle('active', node.id === `page-${tab}`);
            });
        }

        function applyMetrics(data) {
            setNodeText('metricProjects', String((data.projects || []).length));
            setNodeText('metricBuilds', String((data.recent_builds || []).length));
            setNodeText('metricActivity', String((data.recent_activity || []).length));
            const firstProject = (data.projects || [])[0];
            setNodeText('metricCurrentProject', firstProject ? firstProject.name : '未选中');
        }

        async function loadDashboard() {
            const resp = await fetch('/api/dashboard');
            const data = await resp.json();
            if (data.error) {
                if (resp.status === 401) {
                    window.location.href = '/';
                }
                return;
            }
            dashboardState = data;
            renderNotices(data.notices || []);
            renderProjectTable(data.projects || []);
            renderBuildTable(data.recent_builds || []);
            renderActivityTable(data.recent_activity || []);
            applyMetrics(data);
            currentUser = data.user;
            const roleLabel = currentUser.role === 'admin' ? '管理员' : '开发者';
            setNodeText('userStatus', `${currentUser.username} / ${roleLabel}`);
            setNodeText('workspaceSession', `${roleLabel} 会话在线`);
            setNodeText('sessionRole', roleLabel);
            setNodeText('sessionProject', currentProject ? currentProject.id : ((data.projects || [])[0]?.id || '尚未创建'));
            setNodeText('sessionBuild', currentBuild || ((data.recent_builds || [])[0]?.id || '尚未触发'));
            if (currentUser.role === 'admin') {
                document.getElementById('tab-admin').style.display = 'inline-flex';
                renderResult('adminResult', {
                    status: 'admin',
                    scope: 'ops'
                });
            } else {
                document.getElementById('tab-admin').style.display = 'none';
                if (currentTab === 'admin') {
                    switchTab('dashboard');
                }
            }
        }

        async function logout() {
            await fetch('/logout', {method: 'POST'});
            window.location.href = '/';
        }

        async function createProject() {
            const name = document.getElementById('projectName').value;
            const namespace = document.getElementById('projectNamespace').value;
            const template = document.getElementById('projectTemplate').value;
            const team = document.getElementById('projectTeam').value;
            const notify_channel = document.getElementById('projectChannel').value;
            const retention_days = document.getElementById('projectRetention').value;
            const change_window = document.getElementById('projectWindow').value;
            const tags = document.getElementById('projectTags').value;
            const resp = await fetch('/api/project', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({
                    name,
                    namespace,
                    template,
                    team,
                    notify_channel,
                    retention_days,
                    change_window,
                    tags
                })
            });
            const data = await resp.json();
            renderResult('projectResult', data);
            if (data.success) {
                currentProject = data.project;
                document.getElementById('buildProject').value = data.project.id;
                switchTab('projects');
                await loadDashboard();
            }
        }

        async function triggerBuild() {
            const project = document.getElementById('buildProject').value;
            const resp = await fetch('/api/build', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({project})
            });
            const data = await resp.json();
            renderResult('buildResult', data);
            if (data.log) {
                const logBox = document.getElementById('buildLog');
                logBox.style.display = 'block';
                logBox.innerText = data.log;
            }
            if (data.build_id) {
                currentBuild = data.build_id;
                switchTab('builds');
                await loadDashboard();
            }
        }

        switchTab('dashboard');
        loadDashboard();
    </script>
</body>
</html>
HTML;
    $response = new Response($html, 200, ['Content-Type' => 'text/html']);
    $response->send();
    exit;
}

// Login
if ($method === 'POST' && $routePath === '/login') {
    $data = json_decode($request->getContent(), true) ?: [];
    $user = loginUser($data['username'] ?? '', $data['password'] ?? '');

    if ($user) {
        sendJson([
            'success' => true,
            'user' => [
                'username' => $user['username'],
                'role' => $user['role']
            ]
        ]);
    } else {
        sendJson(['success' => false, 'error' => 'Invalid credentials'], 401);
    }
}

if ($method === 'POST' && $routePath === '/logout') {
    logoutUser();
    sendJson(['success' => true]);
}

if ($method === 'GET' && $routePath === '/api/dashboard') {
    $user = requireUser();
    $allData = loadData();
    sendJson(dashboardPayload($user, $allData));
}

// API: Create project
if ($method === 'POST' && $routePath === '/api/project') {
    $user = requireUser();

    $data = json_decode($request->getContent(), true) ?: [];
    $projectId = 'proj_' . bin2hex(random_bytes(8));
    $allData = loadData();
    $allData['projects'][$projectId] = createProjectRecord($projectId, $data, $user);
    appendProjectActivity(
        $allData,
        $projectId,
        $user,
        'project',
        '创建了临时项目工作区',
        [
            'template' => $allData['projects'][$projectId]['template'],
            'team' => $allData['projects'][$projectId]['team'],
            'notify_channel' => $allData['projects'][$projectId]['notify_channel'],
        ]
    );
    saveData($allData);

    sendJson(['success' => true, 'project' => $allData['projects'][$projectId]]);
}

// API: Get project
if ($method === 'GET' && preg_match('#^/api/project/([a-zA-Z0-9_]+)$#', $routePath, $m)) {
    $user = requireUser();

    $allData = loadData();
    if (isset($allData['projects'][$m[1]])) {
        $project = $allData['projects'][$m[1]];
        if (!userCanAccessProject($user, $project)) {
            sendJson(['error' => 'Forbidden'], 403);
        }
        sendJson($project);
    } else {
        sendJson(['error' => 'Project not found'], 404);
    }
}

if ($method === 'POST' && preg_match('#^/api/project/([a-zA-Z0-9_]+)/notes$#', $routePath, $m)) {
    $user = requireUser();
    $allData = loadData();
    $projectId = $m[1];
    if (!isset($allData['projects'][$projectId])) {
        sendJson(['error' => 'Project not found'], 404);
    }

    $project = $allData['projects'][$projectId];
    if (!userCanAccessProject($user, $project)) {
        sendJson(['error' => 'Forbidden'], 403);
    }

    $data = json_decode($request->getContent(), true) ?: [];
    $message = trim((string) ($data['message'] ?? ''));
    if ($message === '') {
        sendJson(['error' => 'Message is required'], 400);
    }

    if (!isset($allData['projects'][$projectId]['notes'])) {
        $allData['projects'][$projectId]['notes'] = [];
    }

    $note = [
        'id' => 'note_' . bin2hex(random_bytes(5)),
        'author' => $user['username'],
        'message' => mb_substr($message, 0, 240),
        'created' => nowStamp(),
    ];
    $allData['projects'][$projectId]['notes'][] = $note;
    appendProjectActivity($allData, $projectId, $user, 'note', '新增了一条项目备注', ['note_id' => $note['id']]);
    saveData($allData);

    sendJson(['success' => true, 'note' => $note]);
}

if ($method === 'POST' && preg_match('#^/api/project/([a-zA-Z0-9_]+)/schedule$#', $routePath, $m)) {
    $user = requireUser();
    $allData = loadData();
    $projectId = $m[1];
    if (!isset($allData['projects'][$projectId])) {
        sendJson(['error' => 'Project not found'], 404);
    }

    $project = $allData['projects'][$projectId];
    if (!userCanAccessProject($user, $project)) {
        sendJson(['error' => 'Forbidden'], 403);
    }

    $data = json_decode($request->getContent(), true) ?: [];
    $window = $data['change_window'] ?? 'business-hours';
    $notifyChannel = $data['notify_channel'] ?? ($project['notify_channel'] ?? '#preview-desk');
    $retentionDays = (int) ($data['retention_days'] ?? ($project['retention_days'] ?? 7));

    $allData['projects'][$projectId]['change_window'] = $window;
    $allData['projects'][$projectId]['notify_channel'] = $notifyChannel;
    $allData['projects'][$projectId]['retention_days'] = max(1, $retentionDays);
    appendProjectActivity(
        $allData,
        $projectId,
        $user,
        'schedule',
        '更新了项目的通知与保留策略',
        [
            'change_window' => $window,
            'notify_channel' => $notifyChannel,
            'retention_days' => $allData['projects'][$projectId]['retention_days'],
        ]
    );
    saveData($allData);

    sendJson([
        'success' => true,
        'project' => summarizeProject($allData['projects'][$projectId])
    ]);
}

// API: Trigger build
if ($method === 'POST' && $routePath === '/api/build') {
    $user = requireUser();

    $data = json_decode($request->getContent(), true) ?: [];
    $projectId = $data['project'] ?? '';

    $allData = loadData();
    if (!isset($allData['projects'][$projectId])) {
        sendJson(['error' => 'Project not found'], 404);
    }

    $project = $allData['projects'][$projectId];
    if (!userCanAccessProject($user, $project)) {
        sendJson(['error' => 'Forbidden'], 403);
    }

    $buildMode = executionProfileForProject($project);
    $transport = transportForProject($project);
    $buildData = [
        'project' => $projectId,
        'profile' => $buildMode,
        'require' => ['php' => '>=8.0'],
        'repositories' => $project['repositories'] ?? [],
        'driver' => $transport,
        'workspace_policy' => $project['workspace_policy'] ?? 'archive',
        'notify_channel' => $project['notify_channel'] ?? '#preview-desk',
        'team' => $project['team'] ?? 'runtime',
    ];

    $startedAt = microtime(true);
    $ch = curl_init(BUILDER_URL . '/build/preview');
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_POST, true);
    curl_setopt($ch, CURLOPT_POSTFIELDS, json_encode($buildData));
    curl_setopt($ch, CURLOPT_HTTPHEADER, ['Content-Type: application/json']);
    curl_setopt($ch, CURLOPT_TIMEOUT, 65);
    $result = curl_exec($ch);
    $httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);

    $buildResult = json_decode($result, true) ?: ['success' => false, 'error' => 'Build service error'];

    // Store build record
    $buildId = 'build_' . bin2hex(random_bytes(8));
    $allData['builds'][$buildId] = [
        'id' => $buildId,
        'project' => $projectId,
        'user' => $user['id'],
        'requested_by' => $user['username'],
        'profile' => $buildData['profile'],
        'driver' => $buildData['driver'],
        'mode_label' => $buildData['profile'],
        'transport_label' => $buildData['driver'],
        'timestamp' => nowStamp(),
        'duration_ms' => (int) round((microtime(true) - $startedAt) * 1000),
        'result' => $buildResult
    ];
    $allData['projects'][$projectId]['builds'][] = $buildId;
    appendProjectActivity(
        $allData,
        $projectId,
        $user,
        'build',
        '触发了一次依赖预览构建',
        [
            'build_id' => $buildId,
            'mode' => $buildData['profile'],
            'transport' => $buildData['driver'],
            'success' => $buildResult['success'] ?? false,
        ]
    );
    saveData($allData);

    sendJson([
        'success' => $buildResult['success'] ?? false,
        'build_id' => $buildId,
        'log' => $buildResult['public_log'] ?? 'Build completed'
    ]);
}

// API: Build status
if ($method === 'GET' && preg_match('#^/api/build/([a-zA-Z0-9_]+)/status$#', $routePath, $m)) {
    $user = requireUser();
    $allData = loadData();
    if (isset($allData['builds'][$m[1]])) {
        $build = $allData['builds'][$m[1]];
        if (!userCanAccessBuild($user, $build)) {
            sendJson(['error' => 'Forbidden'], 403);
        }
        sendJson([
            'id' => $build['id'],
            'project' => $build['project'],
            'status' => 'completed',
            'success' => $build['result']['success'] ?? false
        ]);
    } else {
        sendJson(['error' => 'Build not found'], 404);
    }
}

// API: Build result
if ($method === 'GET' && preg_match('#^/api/build/([a-zA-Z0-9_]+)/result$#', $routePath, $m)) {
    $user = requireUser();
    $allData = loadData();
    if (isset($allData['builds'][$m[1]])) {
        $build = $allData['builds'][$m[1]];
        if (!userCanAccessBuild($user, $build)) {
            sendJson(['error' => 'Forbidden'], 403);
        }
        sendJson([
            'id' => $build['id'],
            'success' => $build['result']['success'] ?? false,
            'log' => $build['result']['public_log'] ?? 'No log available'
        ]);
    } else {
        sendJson(['error' => 'Build not found'], 404);
    }
}

if ($method === 'POST' && $routePath === '/admin/integrations/register') {
    $user = requireUser();

    $data = json_decode($request->getContent(), true) ?: [];
    $repoUrl = $data['url'] ?? '';
    $integration = $data['integration'] ?? ($data['type'] ?? 'composer');
    $repoType = normalizeIntegrationType($integration);
    $repoOptions = $data['options'] ?? [];

    if (!$repoUrl) {
        sendJson(['error' => 'Repository URL is required'], 400);
    }

    $allData = loadData();
    $repoId = 'repo_' . bin2hex(random_bytes(6));
    $allData['repositories'][$repoId] = [
        'id' => $repoId,
        'url' => $repoUrl,
        'type' => $repoType,
        'options' => $repoOptions,
        'registered_by' => $user['id'],
        'registered_at' => nowStamp()
    ];

    if (!empty($data['project'])) {
        if (isset($allData['projects'][$data['project']])) {
            if (!userCanAccessProject($user, $allData['projects'][$data['project']])) {
                sendJson(['error' => 'Forbidden'], 403);
            }
            $allData['projects'][$data['project']]['repositories'][] = [
                'type' => $repoType,
                'url' => $repoUrl,
                'options' => $repoOptions
            ];
            appendProjectActivity(
                $allData,
                $data['project'],
                $user,
                'repository',
                '挂载了新的外部仓库配置',
                [
                    'repository' => $repoId,
                    'type' => $repoType,
                ]
            );
        }
    }

    saveData($allData);

    sendJson([
        'success' => true,
        'repository' => $allData['repositories'][$repoId],
        'message' => 'Integration registered successfully'
    ]);
}

if ($method === 'POST' && $routePath === '/admin/workspace-policy/update') {
    $user = requireUser();

    $data = json_decode($request->getContent(), true) ?: [];
    $projectId = $data['project'] ?? '';
    $mode = normalizeExecutionMode($data['mode'] ?? ($data['profile'] ?? 'workspace'));

    $allData = loadData();
    if (!isset($allData['projects'][$projectId])) {
        sendJson(['error' => 'Project not found'], 404);
    }
    if (!userCanAccessProject($user, $allData['projects'][$projectId])) {
        sendJson(['error' => 'Forbidden'], 403);
    }

    $allData['projects'][$projectId]['workspace_policy'] = $mode === 'workspace' ? 'workspace' : 'archive';
    $allData['projects'][$projectId]['build_profile'] = $mode;
    $allData['projects'][$projectId]['execution_mode'] = $mode;
    appendProjectActivity(
        $allData,
        $projectId,
        $user,
        'policy',
        '调整了工作区保留策略',
        [
            'mode' => $mode,
            'workspace_policy' => $allData['projects'][$projectId]['workspace_policy'],
        ]
    );
    saveData($allData);

    sendJson([
        'success' => true,
        'project' => $projectId,
        'mode' => $mode,
        'workspace_policy' => $allData['projects'][$projectId]['workspace_policy'],
        'message' => "Workspace policy updated for '{$mode}' mode"
    ]);
}

if ($method === 'POST' && $routePath === '/admin/integrations/set-channel') {
    $user = requireUser();

    $data = json_decode($request->getContent(), true) ?: [];
    $projectId = $data['project'] ?? '';
    $channel = $data['channel'] ?? ($data['driver'] ?? 'composer');
    $driver = normalizeIntegrationType($channel);

    $allData = loadData();
    if (!isset($allData['projects'][$projectId])) {
        sendJson(['error' => 'Project not found'], 404);
    }
    if (!userCanAccessProject($user, $allData['projects'][$projectId])) {
        sendJson(['error' => 'Forbidden'], 403);
    }

    $allData['projects'][$projectId]['driver'] = $driver;
    $allData['projects'][$projectId]['transport'] = $driver;
    $allData['projects'][$projectId]['integration_channel'] = $channel;
    appendProjectActivity(
        $allData,
        $projectId,
        $user,
        'transport',
        '更新了项目的拉取通道配置',
        [
            'channel' => $channel,
            'transport' => $driver
        ]
    );
    saveData($allData);

    sendJson([
        'success' => true,
        'project' => $projectId,
        'channel' => $channel,
        'transport' => $allData['projects'][$projectId]['transport'],
        'message' => "Integration channel set to '{$channel}'"
    ]);
}

if ($method === 'GET' && preg_match('#^/admin/build/([a-zA-Z0-9_]+)/log$#', $routePath, $m)) {
    $user = requireUser();
    $allData = loadData();
    $buildId = $m[1] ?? '';

    if (isset($allData['builds'][$buildId])) {
        $build = $allData['builds'][$buildId];
        if (!userCanAccessBuild($user, $build)) {
            sendJson(['error' => 'Forbidden'], 403);
        }

        sendJson([
            'id' => $build['id'],
            'project' => $build['project'],
            'full_log' => $build['result']['log'] ?? 'No log available'
        ]);
    } else {
        sendJson(['error' => 'Build not found'], 404);
    }
}

if ($method === 'GET' && $routePath === '/admin/system/health') {
    requireUser();

    $healthData = [
        'status' => 'healthy',
        'services' => [
            'web' => 'running',
            'php' => PHP_VERSION,
            'symfony' => '6.4.28'
        ],
    ];

    $ch = curl_init(BUILDER_URL . '/health');
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_TIMEOUT, 3);
    $builderHealth = curl_exec($ch);
    curl_close($ch);

    if ($builderHealth) {
        $healthData['services']['builder'] = json_decode($builderHealth, true);
    } else {
        $healthData['services']['builder'] = 'unreachable';
    }

    $ch = curl_init(COORDINATOR_URL . '/health');
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_TIMEOUT, 3);
    $coordHealth = curl_exec($ch);
    curl_close($ch);

    if ($coordHealth) {
        $healthData['services']['coordinator'] = json_decode($coordHealth, true);
    } else {
        $healthData['services']['coordinator'] = 'unreachable';
    }

    sendJson($healthData);
}

if ($method === 'GET' && $routePath === '/internal/artifact/fetch') {
    requireUser();

    $query = http_build_query([
        'timestamp' => $request->query->get('timestamp', ''),
        'action' => $request->query->get('action', ''),
        'signature' => $request->query->get('signature', ''),
    ]);

    $ch = curl_init(COORDINATOR_URL . '/internal/artifact/fetch?' . $query);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_TIMEOUT, 5);
    $result = curl_exec($ch);
    $httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE) ?: 502;
    curl_close($ch);

    $decoded = json_decode($result ?: '', true);
    if ($decoded === null) {
        sendJson(['error' => 'Coordinator unavailable'], 502);
    }

    sendJson($decoded, $httpCode);
}

$response = new JsonResponse([
    'error' => 'Not found',
    'path' => $pathInfo,
    'method' => $method
], 404);
$response->send();
