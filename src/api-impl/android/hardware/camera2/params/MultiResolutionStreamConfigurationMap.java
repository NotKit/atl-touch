package android.hardware.camera2.params;

import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * Which formats a logical camera can stream at more than one resolution.
 *
 * Nothing builds one yet: a session configures a single backend stream, so
 * there is no multi-resolution output to describe even where the HAL lists
 * physical sub-cameras. The class exists because apps ask for it and must get
 * a real, empty answer rather than a missing class.
 */
public final class MultiResolutionStreamConfigurationMap {

	private final Map<Integer, Collection<MultiResolutionStreamInfo>> outputs;
	private final Map<Integer, Collection<MultiResolutionStreamInfo>> inputs;

	public MultiResolutionStreamConfigurationMap(
	    Map<Integer, Collection<MultiResolutionStreamInfo>> outputs,
	    Map<Integer, Collection<MultiResolutionStreamInfo>> inputs) {
		this.outputs = outputs == null
		    ? new HashMap<Integer, Collection<MultiResolutionStreamInfo>>() : outputs;
		this.inputs = inputs == null
		    ? new HashMap<Integer, Collection<MultiResolutionStreamInfo>>() : inputs;
	}

	public int[] getOutputFormats() {
		return formats(outputs);
	}

	public int[] getInputFormats() {
		return formats(inputs);
	}

	public Collection<MultiResolutionStreamInfo> getOutputInfo(int format) {
		return info(outputs, format);
	}

	public Collection<MultiResolutionStreamInfo> getInputInfo(int format) {
		return info(inputs, format);
	}

	@Override
	public String toString() {
		return "MultiResolutionStreamConfigurationMap(" + outputs.size() + " output format(s))";
	}

	private static int[] formats(Map<Integer, Collection<MultiResolutionStreamInfo>> map) {
		int[] out = new int[map.size()];
		int at = 0;

		for (Integer format : map.keySet())
			out[at++] = format.intValue();
		return out;
	}

	private static Collection<MultiResolutionStreamInfo> info(
	    Map<Integer, Collection<MultiResolutionStreamInfo>> map, int format) {
		Collection<MultiResolutionStreamInfo> found = map.get(Integer.valueOf(format));

		return found == null ? Collections.<MultiResolutionStreamInfo>emptyList() : found;
	}
}
