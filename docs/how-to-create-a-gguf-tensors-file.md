# How to create a gguf file

For this example we use a ```TINY SD TURBO``` model provided by user ```cc-nms```  on Hugging Face: 

### 1. Get model files
Download the diffusers model from Hugging Face using Python:
```python
from diffusers import StableDiffusionPipeline
pipe = StableDiffusionPipeline.from_pretrained("cc-nms/tiny-sd-turbo")
pipe.save_pretrained(save_directory="tiny-sd-turbo")
```
Also you could simply use you browser and download some files: go to https://huggingface.co/cc-nms/tiny-sd-turbo/tree/main and place the contents of ```text_encoder, unet and  vae``` in three folders below ./tiny-sd-turbo .

### 2. Create a safetensors file
```bash
python convert_diffusers_to_original_stable_diffusion.py \
    --model_path  ./tiny-sd-turbo \
    --checkpoint_path tiny-sd-turbo.safetensors \
    --half --use_safetensors
```
### 3. Create a gguf file:
```bash
build/bin/sd \
    -M convert -m tiny-sd-turbo.safetensors \
    -o tiny-sd-turbo_q8_0.gguf -v --type q8_0
```

