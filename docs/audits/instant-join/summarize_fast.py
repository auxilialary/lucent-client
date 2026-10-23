"""Summarize fast-join probes; p95 is nearest-rank (n=10 implies p95=max)."""
import json, math, statistics, sys
from collections import defaultdict
from pathlib import Path

def summarize(path):
    grouped = defaultdict(lambda: defaultdict(list))
    samples = json.loads(Path(path).read_text())
    for sample in samples:
        events = {x['event']: x['ns'] for x in sample['events'] if 'event' in x}
        scopes = defaultdict(list)
        for x in sample['events']:
            if 'scope' in x:
                scopes[x['scope']].append(x['ms'])
        start = events.get('connect', events.get('map_switch'))
        metrics = {name: (events[event] - start)/1e6 for name,event in
            [('online_proxy','snap2'),('playable_submission','swap_submitted'),('full_submission','full_scene_submitted')]}
        metrics['critical_resources'] = (events['resources_done']-events['con_ready'])/1e6
        metrics['snapshot_gap'] = (events['snap2']-events['snap1'])/1e6
        metrics['first_snapshot_wait'] = (events['snap1']-events['enter_game'])/1e6
        metrics['resource_work'] = sum(scopes['on_connected'] + scopes['deferred_image'] + scopes['deferred_layer'])
        for name in ['max_network_gap','deferred_network_gap','online_network_gap','deferred_image','deferred_layer']:
            if scopes[name]: metrics[name] = max(scopes[name])
        if 'decorative_ready' in events:
            metrics['resources_wall'] = (events['decorative_ready']-events['con_ready'])/1e6
        if sample.get('peak_working_set_bytes'):
            metrics['peak_working_set_mib'] = sample['peak_working_set_bytes']/2**20
        for name, value in metrics.items(): grouped[sample['protocol']+'/'+sample['case']][name].append(value)
    def stats(values):
        return dict(n=len(values), p50=statistics.median(values),p95=sorted(values)[math.ceil(len(values)*.95)-1],max=max(values))
    return {group:{metric:stats(values) for metric,values in metrics.items()} for group,metrics in grouped.items()}

if __name__ == '__main__':
    result = {Path(p).stem:summarize(p) for p in sys.argv[1:]}
    print(json.dumps(result,indent=2))
