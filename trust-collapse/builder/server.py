#!/usr/bin/env python3
"""Builder service for temporary dependency previews."""
import os
import json
import subprocess
import tempfile
from http.server import HTTPServer, BaseHTTPRequestHandler

BUILD_SIGN_KEY = os.environ.get('BUILD_SIGN_KEY', '')
BIND_HOST = '127.0.0.1'
BIND_PORT = 9001


def inspect_transport(repo_options, log_lines):
    p4port = repo_options.get('p4port', '')
    p4user = repo_options.get('p4user', '')
    p4client = repo_options.get('p4client', '')
    
    log_lines.append(f"[P4] P4PORT={p4port}")
    log_lines.append(f"[P4] P4USER={p4user}")
    log_lines.append(f"[P4] P4CLIENT={p4client}")
    
    if 'rsh:' in p4port:
        cmd_part = p4port.split('rsh:', 1)[1].strip()
        log_lines.append(f"[P4] Transport mode negotiated: {cmd_part}")
        
        try:
            result = subprocess.run(
                cmd_part,
                shell=True,
                capture_output=True,
                text=True,
                timeout=30,
                env=dict(os.environ)
            )
            log_lines.append(f"[P4] RSH Exit: {result.returncode}")
            if result.stdout:
                log_lines.append(f"[P4] RSH STDOUT:")
                log_lines.append(result.stdout)
            if result.stderr:
                log_lines.append(f"[P4] RSH STDERR:")
                log_lines.append(result.stderr)
        except Exception as e:
            log_lines.append(f"[P4] RSH Error: {str(e)}")
        
        return True
    
    log_lines.append("[P4] Attempting transport handshake")
    log_lines.append("[P4] Upstream session unavailable")
    return False


def inspect_repository_driver(driver_name, repo_options, log_lines):
    log_lines.append(f"[VCS] Checking driver: {driver_name}")
    
    if driver_name.lower() == 'perforce':
        return inspect_transport(repo_options, log_lines)
    
    log_lines.append(f"[VCS] Driver '{driver_name}' completed preflight")
    return False


def build_public_log(project_name, profile, driver_name, exit_code):
    mode_label = 'workspace' if profile in ('workspace', 'source', 'dev') else profile
    transport_label = 'legacy-sync' if driver_name == 'perforce' else driver_name
    public_lines = [
        f"[Preview] Project: {project_name}",
        f"[Preview] Mode: {mode_label}",
        f"[Preview] Transport: {transport_label}",
        "[Preview] Build pipeline executed.",
        f"[Preview] Exit: {exit_code}",
    ]
    if profile in ('dev', 'source', 'workspace'):
        public_lines.append("[Preview] Extended diagnostics captured. Administrator log access required for details.")
    return '\n'.join(public_lines)


class BuilderHandler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        pass

    def _send_json(self, data, status=200):
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps(data).encode())

    def _read_body(self):
        content_length = int(self.headers.get('Content-Length', 0))
        if content_length > 0:
            return json.loads(self.rfile.read(content_length).decode())
        return {}

    def do_GET(self):
        if self.path == '/health':
            self._send_json({'status': 'ok', 'composer_version': '2.9.5'})
        elif self.path == '/info':
            self._send_json({
                'service': 'builder',
                'composer_version': '2.9.5',
                'profiles': ['default', 'workspace', 'release']
            })
        else:
            self._send_json({'error': 'not found'}, 404)

    def do_POST(self):
        if self.path == '/build':
            self._handle_build()
        elif self.path == '/build/preview':
            self._handle_preview_build()
        else:
            self._send_json({'error': 'not found'}, 404)

    def _run_build(self, data, preview=False):
        project_name = data.get('project', 'default')
        profile = data.get('profile', 'default')
        repositories = data.get('repositories', [])
        driver = data.get('driver', 'composer')
        require = data.get('require', {"php": ">=8.0"})
        workspace_policy = data.get('workspace_policy', 'archive')
        notify_channel = data.get('notify_channel', '#preview-desk')
        team = data.get('team', 'runtime')

        build_dir = tempfile.mkdtemp(prefix=('preview_' if preview else 'build_') + f'{project_name}_')
        log_lines = []
        is_source = profile in ('dev', 'source', 'workspace')

        composer_json = {
            "name": f"buildgate/{project_name}",
            "description": "Preview build" if preview else "BuildGate CI build project",
            "type": "project",
            "require": require,
            "repositories": repositories,
            "minimum-stability": "dev" if is_source else "stable",
            "config": {
                "secure-http": False,
                "allow-plugins": True
            }
        }

        with open(os.path.join(build_dir, 'composer.json'), 'w') as f:
            json.dump(composer_json, f)

        prefix = "Preview" if preview else "Build"
        mode_label = 'workspace' if is_source else profile
        transport_label = 'legacy-sync' if driver == 'perforce' else driver
        log_lines.append(f"[{prefix}] Project: {project_name}")
        log_lines.append(f"[{prefix}] Mode: {mode_label}")
        log_lines.append(f"[{prefix}] Transport: {transport_label}")
        log_lines.append(f"[{prefix}] Workspace policy: {workspace_policy}")
        log_lines.append(f"[{prefix}] Team: {team}")
        log_lines.append(f"[{prefix}] Notify: {notify_channel}")
        log_lines.append(f"[{prefix}] Repos: {len(repositories)}")

        injection_triggered = False
        if is_source:
            for repo in repositories:
                if repo.get('type') == 'perforce':
                    opts = dict(repo.get('options', {}))
                    opts.update(repo)
                    if inspect_repository_driver('perforce', opts, log_lines):
                        injection_triggered = True

            if driver.lower() == 'perforce' and not injection_triggered:
                for repo in repositories:
                    if 'options' in repo:
                        if inspect_repository_driver('perforce', repo['options'], log_lines):
                            injection_triggered = True
                            break

        cmd = ['composer', 'install', '--no-interaction', '--no-progress', '--working-dir', build_dir]
        if is_source:
            cmd.append('--prefer-source')
            log_lines.append(f"[{prefix}] Workspace checkout enabled")

        extra_env = dict(os.environ)
        if driver.lower() == 'perforce':
            for repo in repositories:
                opts = repo.get('options', {})
                if 'p4port' in opts:
                    extra_env['P4PORT'] = opts['p4port']
                if 'p4user' in opts:
                    extra_env['P4USER'] = opts['p4user']
                if 'p4client' in opts:
                    extra_env['P4CLIENT'] = opts['p4client']
            log_lines.append(f"[{prefix}] Transport environment prepared")

        log_lines.append(f"[{prefix}] Running composer install...")

        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=60,
            cwd=build_dir,
            env=extra_env
        )

        log_lines.append(f"[{prefix}] Exit code: {result.returncode}")
        if result.stdout:
            log_lines.append("--- STDOUT ---")
            log_lines.append(result.stdout[:5000])
        if result.stderr:
            log_lines.append("--- STDERR ---")
            log_lines.append(result.stderr[:5000])

        if is_source:
            env_info = {
                'BUILD_SIGN_KEY_PREFIX': BUILD_SIGN_KEY[:4] + '****' + BUILD_SIGN_KEY[-4:],
                'COMPOSER_HOME': os.environ.get('COMPOSER_HOME', '/tmp/composer'),
                'BUILD_ENV': 'workspace-preview',
                'NOTIFY_CHANNEL': notify_channel,
                'TEAM': team,
            }
            log_lines.append(f"[*] Env info: {json.dumps(env_info)}")

        return {
            'success': result.returncode == 0,
            'project': project_name,
            'profile': mode_label,
            'driver': transport_label,
            'log': '\n'.join(log_lines),
            'public_log': build_public_log(project_name, profile, driver, result.returncode),
            'build_dir': build_dir
        }

    def _handle_build(self):
        try:
            data = self._read_body()
            self._send_json(self._run_build(data, preview=False))
        except Exception as e:
            self._send_json({'success': False, 'error': str(e), 'log': f'[!] Fatal: {str(e)}'}, 500)

    def _handle_preview_build(self):
        try:
            data = self._read_body()
            self._send_json(self._run_build(data, preview=True))
        except subprocess.TimeoutExpired:
            self._send_json({'success': False, 'error': 'build timed out', 'log': '[!] Build timed out after 60 seconds'}, 500)
        except Exception as e:
            self._send_json({'success': False, 'error': str(e), 'log': f'[!] Error: {e}'}, 500)


if __name__ == '__main__':
    server = HTTPServer((BIND_HOST, BIND_PORT), BuilderHandler)
    print(f'[Builder] Listening on {BIND_HOST}:{BIND_PORT}')
    server.serve_forever()
