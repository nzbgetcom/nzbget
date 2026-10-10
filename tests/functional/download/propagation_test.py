import re
import time

nzbget_options = ['PropagationDelay=15', 'DetailTarget=both']

def make_nzb(nzbget):
	# small.dat was posted an hour ago, the obfuscated file just now (like a late nfo or par2)
	nzb_content = nzbget.load_nzb('small.nzb')
	nzb_content = re.sub(r'date="\d+"', 'date="%i"' % (time.time() - 3600), nzb_content)
	late_file = re.search(r'<file .*?</file>', nzbget.load_nzb('small-obfuscated.nzb'), re.S).group(0)
	late_file = re.sub(r'date="\d+"', 'date="%i"' % time.time(), late_file)
	return nzb_content.replace('</nzb>', late_file + '\n</nzb>')

def wait_until(condition, timeout = 30):
	deadline = time.time() + timeout
	while time.time() < deadline:
		result = condition()
		if result:
			return result
		time.sleep(0.2)
	raise Exception('Timeout')

def find_group(nzbget, nzb_name):
	for group in nzbget.api.listgroups():
		if group['NZBFilename'] == nzb_name:
			return group
	return None

def test_propagation_delay_holds_late_file(nserv, nzbget):
	nzb_name = 'propagation.nzb'
	nzbget.append_nzb(nzb_name, make_nzb(nzbget))
	nzb_id = wait_until(lambda: find_group(nzbget, nzb_name))['NZBID']

	# the old file gets downloaded, the late one stays in the queue
	files = wait_until(lambda: (lambda files: files if len(files) == 1 else None)(nzbget.api.listfiles(0, 0, nzb_id)))
	assert files[0]['Filename'] == 'fsdkhKHGuwuMNBKskd'
	assert not files[0]['Paused']

	group = find_group(nzbget, nzb_name)
	assert group['Status'] == 'QUEUED'
	assert group['ActiveDownloads'] == 0
	# the web-interface shows the propagation label based on the newest post
	assert group['MaxPostTime'] - group['MinPostTime'] >= 3000

	# the hold-back is logged once, so the queue doesn't look stuck without explanation
	holding = 'fsdkhKHGuwuMNBKskd due to PropagationDelay'
	wait_until(lambda: [m for m in nzbget.api.log(0, 1000) if holding in m['Text']], timeout = 10)
	time.sleep(3)
	assert len([m for m in nzbget.api.log(0, 1000) if holding in m['Text']]) == 1

	# resuming doesn't help, the file isn't paused
	nzbget.api.editqueue('GroupResume', '', [nzb_id])
	time.sleep(1)
	assert find_group(nzbget, nzb_name)['Status'] == 'QUEUED'
	assert len(nzbget.api.listfiles(0, 0, nzb_id)) == 1

	nzbget.api.editqueue('GroupFinalDelete', '', [nzb_id])
