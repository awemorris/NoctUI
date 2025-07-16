NoctUI
======

This is a demonstration of a GTK4 Declaretive UI with Noct language.

```
func main() {
     UIRun(
         // Title
         "Hello",

         // Window Size
	 640, 480,

         // Declative UI
         VBox([
		Label({
		    title: "Hello, GNU-style Declative UI!"
		}),
		Button({
		    title: "Push Here",
		    onClick: lambda () => {
                        print("Clicked!");
		    }
		})
	 ])
     );
}
```

![screenshot.png](screenshot.png)
