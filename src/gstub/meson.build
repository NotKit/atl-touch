srcs = [
	'com/google/android/gms/providerinstaller/ProviderInstallerImpl.java',
]
# same compilation model as src/api-impl: the JDK's own java.*, with the
# framework jar on the class path for the android.* classes the stubs use
java_args = [
	'--release', '8',
	'-encoding', 'UTF-8',
]

ant = find_program('ant', required: false)

if ant.found()
	gstub_jar = custom_target('gstub',
	                          output: 'gstub.jar',
	                          command: [ant,
	                                   '-buildfile', files('build.xml'),
	                                   '-Dbasedir=' + builddir_base,
	                                   '-Dsrcdir=' + meson.current_source_dir(),
	                                   '-Dincludes=' + ','.join(srcs),
	                                   '-Djarfile=@OUTPUT@',
	                                   '-Dbuild=@PRIVATE_DIR@',
	                                   '-Djavaargs=' + ' '.join(java_args + ['-classpath', hax_jar.full_path()]),
	                          ],
	                          depends: [hax_jar],
	                          depend_files: srcs + ['build.xml'],
	)
else
	gstub_jar = jar('gstub', srcs,
	                link_with: [hax_jar],
	                java_args: java_args,)
endif
