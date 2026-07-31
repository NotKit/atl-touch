package android.view.textclassifier;

/**
 * Text classification service. ATL classifies nothing, so the only instance is
 * NO_OP; this exists because TextView-derived widgets declare
 * setTextClassifier(TextClassifier), and reflecting over their declared methods
 * loads the parameter type.
 */
public interface TextClassifier {
	TextClassifier NO_OP = new TextClassifier() {};
}
