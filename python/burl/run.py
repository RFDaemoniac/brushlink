from burl.language import Context, EvaluationError, Value
from burl.parser import ParseNode

def run_module(module, function, args = []):
	context = Context()
	ast = ParseNode.parse("""
LoadModule %s
%s %s""" % (module, function, " ".join(args)))
	try:
		result = ast.evaluate(context)
		if result.element_type == "ValueName" and result.value == function:
			# will throw an error if the function doesn't exist
			context.get_definition(function)
		print(result)
	except EvaluationError as e:
		print("EvaluationError: %s" % (e.__str__()))


def run_repl():
	repl = Context()
	first_prompt = ": "
	second_prompt = "| "
	lines = []
	index = 0
	while True:
		if not lines:
			prompt = str(index)
			prompt = "$" + prompt
			line = input(prompt + first_prompt)
			if line:
				if line == "Exit":
					return
				else:
					lines.append(line)
		else:
			prompt = " " * len(str(index)) + " ";
			line = input(prompt + second_prompt)
			if line:
				lines.append(line)
			else:
				print ("\033[A                             \033[A")
				try:
					ast = ParseNode.parse("\n".join(lines))
					value = ast.evaluate(repl)
					name = Value("$" + str(index), "ValueName")
					print(name.value + " = " + value.__str__())
					index += 1
					repl.set(name, value)
				except EvaluationError as e:
					print("EvaluationError: " + e.__str__())
				lines = []


def run_compose(compose_lines=None):
	# todo: can we split cursor and active_tab into their own context separate from the cursor evaluation?
	repl = Context()
	ParseNode.parse("LoadModule composition").evaluate(repl)
	prompt = ": "
	cursor = None
	active_tab = None
	tabs = frozenset()
	force_update_active_tab = False
	line_index = 0
	def maybe_print(s):
		if compose_lines is None or len(compose_lines) < line_index:
			print(s)
	value = None
	while True:
		print(chr(27) + "[2J" + chr(27) + "[H", end="")
		if cursor is None:
			cursor = repl.parse_eval("Set cursor Cursor.Make Quote Sequence")
			active_tab = Value("Any", "Type")
			repl.set(Value("active_tab", "ValueName"), active_tab)
			tabs = repl.parse_eval("AllTypes")
			tabs = [t.value for t in tabs.value]

		options = repl.parse_eval("Cursor.GetAllowedArgumentTypes Get cursor")
		if not options.value: # None or empty frozenset
			maybe_print("No open parameter at cursor")
			elements = []
			value_names = []
		else:
			maybe_print("Allowed Types: " + " ".join([o.value for o in options.value]))
			if force_update_active_tab or (active_tab != "Any"
					and options.value is not None
					and active_tab not in options.value
					and Value('Any', 'Type') not in options.value):
				active_tab = next(iter(options.value))
				repl.set(Value("active_tab", "ValueName"), active_tab)
				force_update_active_tab = False

			elements = repl.parse_eval("DefinitionsOfType Get active_tab")
			elements = [e.value for e in elements.value]
			value_names = repl.parse_eval("ValuesOfType Get active_tab")
			value_names = [n.value for n in value_names.value]

		maybe_print("### Meta ###")
		maybe_print("   Exit Eval Skip Tab <type>")

		maybe_print("### Tabs ###")
		maybe_print("   " + " ".join(tabs))

		if options.value:
			if elements:
				maybe_print("### Elements (%s) ###" % active_tab.value)
				maybe_print("   " + " ".join(elements))
			if value_names:
				maybe_print("### Values (%s) ###" % active_tab.value)
				maybe_print("   " + " ".join(value_names))

		maybe_print("### Command ###")
		maybe_print(cursor.value)

		try:
			if compose_lines is not None and len(compose_lines) > line_index:
				line = compose_lines[line_index]
				line_index += 1
			else:
				line = input(prompt)
			if line == "Exit":
				break
			if line == "Eval":
				try:
					value = repl.parse_eval("Evaluate Cursor.GetEvalNode Get cursor")
					maybe_print(value)
					cursor = None
					force_update_active_tab = True
				except EvaluationError as e:
					maybe_print(e)
			elif line == "Skip":
				success = cursor.value.increment_path_to_open_parameter(force_one=True)
				force_update_active_tab = True
				if not success:
					maybe_print("Cursor could not skip")

			elif line.startswith("Tab "):
				tab = Value(line.split()[-1], "Type")
				if options.value is None or (
					tab not in options.value and Value("Any", "Type") not in options.value):
					maybe_print("Cursor does not allow values of type %s" % tab.value)
				elif repl.is_known_type(tab.value):
						active_tab = tab
						repl.set(Value("active_tab", "ValueName"), active_tab)
				else:
					maybe_print("Uknown Type " + tab.value + " has no elements")

			elif line in elements:
				repl.parse_eval("Cursor.InsertArgument Get cursor Quote " + line)
				force_update_active_tab = True
			elif line in value_names:
				repl.parse_eval("Cursor.InsertArgument Get cursor Quote Get " + line)
				force_update_active_tab = True
			elif active_tab.value == "ValueName":
				repl.parse_eval("Cursor.InsertArgument Get cursor Quote " + line)
				force_update_active_tab = True
			elif active_tab.value == "Type":
				if repl.is_known_type(line):
					repl.parse_eval("Cursor.InsertArgument Get cursor Quote " + line)
					force_update_active_tab = True
				else:
					maybe_print("Uknown Type " + line)
			elif active_tab.value == "Number":
				repl.parse_eval("Cursor.InsertArgument Get cursor Quote " + line)
				force_update_active_tab = True
		except EvaluationError as e:
			print("EvaluationError: " + e.__str__())

		if compose_lines is not None and line_index == len(compose_lines):
			return value
